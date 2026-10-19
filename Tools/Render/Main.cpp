// sidstep2-render: headless host that loads a plugin (VST3 / LV2), plays a MIDI
// file (or a built-in test riff) through it offline and writes a WAV file.
//
// Intended for scripted / LLM-driven use where no DAW or display is available.
//
//   sidstep2-render --plugin build/.../SIDStep2.vst3 --midi song.mid --out song.wav
//   sidstep2-render --plugin SIDStep2.vst3 --test --out test.wav
//   sidstep2-render --plugin SIDStep2.vst3 --list-params
//
// Options:
//   --plugin <path>        plugin bundle (.vst3 or .lv2) or file
//   --midi <file.mid>      MIDI file to render
//   --test                 render a short built-in riff on voices 1-3 (MIDI ch 1-3)
//   --out <file.wav>       output WAV (default: out.wav)
//   --rate <hz>            sample rate (default 44100)
//   --block <n>            block size (default 512)
//   --tail <seconds>       extra time after the last MIDI event (default 2)
//   --bits <16|24|32>      WAV bit depth (default 16)
//   --param <name>=<0..1>  set a parameter (normalised value) by name, repeatable
//   --state-in <file>      load plugin state (as saved by --state-out) before rendering
//   --state-out <file>     save plugin state after rendering
//   --list-params          print parameters and exit

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include <iostream>
#include <map>

using namespace juce;

namespace
{
int fail (const String& message)
{
    std::cerr << "error: " << message << std::endl;
    return 1;
}

MidiMessageSequence makeTestSequence()
{
    MidiMessageSequence seq;
    // Bass on voice 1, arpeggio on voice 2, lead on voice 3. Times in seconds.
    const int bass[]  = { 36, 36, 43, 41 };
    const int arp[]   = { 60, 64, 67, 72 };
    const int lead[]  = { 72, 74, 76, 79 };

    for (int bar = 0; bar < 4; ++bar)
    {
        const double t = bar * 1.0;
        seq.addEvent (MidiMessage::noteOn  (1, bass[bar], (uint8) 110), t);
        seq.addEvent (MidiMessage::noteOff (1, bass[bar]), t + 0.9);

        for (int step = 0; step < 4; ++step)
        {
            const double s = t + step * 0.25;
            seq.addEvent (MidiMessage::noteOn  (2, arp[step], (uint8) 100), s);
            seq.addEvent (MidiMessage::noteOff (2, arp[step]), s + 0.2);
        }

        seq.addEvent (MidiMessage::noteOn  (3, lead[bar], (uint8) 100), t + 0.5);
        seq.addEvent (MidiMessage::noteOff (3, lead[bar]), t + 0.95);
    }
    seq.updateMatchedPairs();
    return seq;
}

bool loadMidiFile (const File& file, MidiMessageSequence& out)
{
    FileInputStream in (file);
    if (! in.openedOk())
        return false;

    MidiFile mf;
    if (! mf.readFrom (in))
        return false;

    mf.convertTimestampTicksToSeconds();
    for (int t = 0; t < mf.getNumTracks(); ++t)
        out.addSequence (*mf.getTrack (t), 0.0);

    out.updateMatchedPairs();
    return true;
}
} // namespace

int main (int argc, char* argv[])
{
    // Plugins need a message manager (and on Linux, its event loop) to exist.
    ScopedJuceInitialiser_GUI juceInit;

    StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (String::fromUTF8 (argv[i]));

    auto valueOf = [&] (const String& flag, const String& fallback = {}) -> String
    {
        const int i = args.indexOf (flag);
        return (i >= 0 && i + 1 < args.size()) ? args[i + 1] : fallback;
    };

    if (args.isEmpty() || args.contains ("--help") || args.contains ("-h"))
    {
        std::cout << "usage: sidstep2-render --plugin <SIDStep2.vst3> (--midi <file.mid> | --test) [--out out.wav]\n"
                     "       [--rate 44100] [--block 512] [--tail 2] [--bits 16] [--param name=value]...\n"
                     "       [--state-in file] [--state-out file] [--list-params]\n";
        return args.isEmpty() ? 1 : 0;
    }

    const auto pluginPath = valueOf ("--plugin");
    if (pluginPath.isEmpty())
        return fail ("--plugin is required");

    const double sampleRate = valueOf ("--rate", "44100").getDoubleValue();
    const int blockSize     = valueOf ("--block", "512").getIntValue();
    const double tail       = valueOf ("--tail", "2").getDoubleValue();
    const int bits          = valueOf ("--bits", "16").getIntValue();

    if (sampleRate <= 0.0)
        return fail ("--rate must be positive");
    if (blockSize <= 0)
        return fail ("--block must be positive");
    if (tail < 0.0)
        return fail ("--tail must not be negative");
    if (bits != 16 && bits != 24 && bits != 32)
        return fail ("--bits must be 16, 24 or 32");

    AudioPluginFormatManager formats;
    addDefaultFormatsToManager (formats);

    const auto pluginFile = File::getCurrentWorkingDirectory().getChildFile (pluginPath);
    OwnedArray<PluginDescription> found;
    for (auto* format : formats.getFormats())
    {
        if (format->fileMightContainThisPluginType (pluginFile.getFullPathName()))
            format->findAllTypesForFile (found, pluginFile.getFullPathName());
        if (! found.isEmpty())
            break;
    }
    if (found.isEmpty())
        return fail ("no plugin found at " + pluginFile.getFullPathName());

    String error;
    auto plugin = formats.createPluginInstance (*found[0], sampleRate, blockSize, error);
    if (plugin == nullptr)
        return fail ("could not load plugin: " + error);

    // Mono or stereo output, no inputs.
    {
        AudioProcessor::BusesLayout layout;
        layout.outputBuses.add (AudioChannelSet::stereo());
        if (! plugin->setBusesLayout (layout))
        {
            layout.outputBuses.getReference (0) = AudioChannelSet::mono();
            plugin->setBusesLayout (layout);
        }
    }

    const auto& params = plugin->getParameters();

    if (args.contains ("--list-params"))
    {
        std::cout << found[0]->name << " (" << found[0]->pluginFormatName << "), "
                  << params.size() << " parameters:\n";
        for (auto* p : params)
            std::cout << "  [" << p->getParameterIndex() << "] " << p->getName (64)
                      << " = " << p->getValue() << " (" << p->getCurrentValueAsText() << ")\n";
        return 0;
    }

    plugin->setNonRealtime (true);
    plugin->setPlayConfigDetails (0, plugin->getTotalNumOutputChannels(), sampleRate, blockSize);
    plugin->prepareToPlay (sampleRate, blockSize);

    if (const auto stateIn = valueOf ("--state-in"); stateIn.isNotEmpty())
    {
        MemoryBlock state;
        if (! File::getCurrentWorkingDirectory().getChildFile (stateIn).loadFileAsData (state))
            return fail ("could not read state file " + stateIn);
        plugin->setStateInformation (state.getData(), (int) state.getSize());
    }

    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] != "--param" || i + 1 >= args.size())
            continue;
        const auto spec  = args[i + 1];
        if (! spec.containsChar ('='))
            return fail ("--param expects name=value, got '" + spec + "'");
        const auto name  = spec.upToLastOccurrenceOf ("=", false, false).trim();
        const auto value = spec.fromLastOccurrenceOf ("=", false, false).getFloatValue();
        bool set = false;
        for (auto* p : params)
        {
            if (p->getName (64).equalsIgnoreCase (name))
            {
                p->setValueNotifyingHost (jlimit (0.0f, 1.0f, value));
                set = true;
            }
        }
        if (! set)
            return fail ("unknown parameter '" + name + "' (use --list-params)");
    }

    MidiMessageSequence sequence;
    if (const auto midiPath = valueOf ("--midi"); midiPath.isNotEmpty())
    {
        if (! loadMidiFile (File::getCurrentWorkingDirectory().getChildFile (midiPath), sequence))
            return fail ("could not read MIDI file " + midiPath);
    }
    else if (args.contains ("--test"))
    {
        sequence = makeTestSequence();
    }
    else
    {
        return fail ("pass --midi <file.mid> or --test");
    }

    const double lengthSeconds = sequence.getEndTime() + tail;
    const auto totalSamples    = (int64) std::ceil (lengthSeconds * sampleRate);
    const int numChannels      = jmax (1, plugin->getTotalNumOutputChannels());

    const auto outFile = File::getCurrentWorkingDirectory().getChildFile (valueOf ("--out", "out.wav"));
    outFile.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<OutputStream> stream (outFile.createOutputStream());
    if (stream == nullptr)
        return fail ("could not write " + outFile.getFullPathName());

    // On success the writer takes ownership of the stream.
    auto writer = wav.createWriterFor (stream, AudioFormatWriterOptions{}.withSampleRate (sampleRate)
                                                                         .withNumChannels (numChannels)
                                                                         .withBitsPerSample (bits));
    if (writer == nullptr)
        return fail ("could not create WAV writer");

    AudioBuffer<float> block (numChannels, blockSize);
    float peak = 0.0f;
    MidiBuffer midi;

    int eventIndex = 0;
    for (int64 pos = 0; pos < totalSamples; pos += blockSize)
    {
        const int n = (int) jmin ((int64) blockSize, totalSamples - pos);
        block.setSize (numChannels, n, false, false, true);
        block.clear();
        midi.clear();

        const double blockEnd = (double) (pos + n) / sampleRate;
        while (eventIndex < sequence.getNumEvents())
        {
            const auto& msg = sequence.getEventPointer (eventIndex)->message;
            if (msg.getTimeStamp() >= blockEnd)
                break;
            if (! msg.isMetaEvent())
            {
                const auto offset = (int) jlimit ((int64) 0, (int64) n - 1, (int64) std::llround (msg.getTimeStamp() * sampleRate) - pos);
                midi.addEvent (msg, offset);
            }
            ++eventIndex;
        }

        plugin->processBlock (block, midi);

        if (! writer->writeFromAudioSampleBuffer (block, 0, n))
            return fail ("could not write " + outFile.getFullPathName());
        peak = jmax (peak, block.getMagnitude (0, n));
    }
    writer.reset();

    plugin->releaseResources();

    if (const auto stateOut = valueOf ("--state-out"); stateOut.isNotEmpty())
    {
        MemoryBlock state;
        plugin->getStateInformation (state);
        if (! File::getCurrentWorkingDirectory().getChildFile (stateOut).replaceWithData (state.getData(), state.getSize()))
            return fail ("could not write state file " + stateOut);
    }

    std::cout << "wrote " << outFile.getFullPathName() << " (" << numChannels << " ch, "
              << String (lengthSeconds, 2) << " s, peak " << String (peak, 4) << ")\n";

    plugin = nullptr;
    return 0;
}
