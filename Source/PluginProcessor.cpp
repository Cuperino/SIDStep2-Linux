#include "PluginProcessor.h"

#include "Defines.h"
#include "MIDIProcessor.h"
#include "ParameterProcessor.h"
#include "PluginEditor.h"
#include "SIDStep2.h"
#include "UI/LiveMode.h"
#include "UI/PatchEditor.h"

#include "Parameters/BooleanParameter.h"
#include "Parameters/SignedPercentParameter.h"
#include "Parameters/UnsignedIntParameter.h"
#include "Parameters/UnsignedPercentParameter.h"
//#include "UI/ExportManager.h"

Sidstep2AudioProcessor::Sidstep2AudioProcessor ()
    :
    AudioProcessor (
                    BusesProperties () . withOutput (
                                                     "Output"
                                                   , AudioChannelSet::stereo ()
                                                   , true ) )
{
    dispatcher = std::make_shared < EventDispatcher > ();

    //exportManager = std::make_shared < ExportManager > ();
    patchEditor = std::make_shared < PatchEditor > (
                                                    dispatcher );
    liveMode = std::make_shared < LiveMode > (
                                              dispatcher );
    core = std::make_shared < SidStep2 > (
                                          dispatcher );
    midiProcessor = std::make_shared < MidiProcessor > (
                                                        dispatcher );
    parameterProcessor = std::make_shared < ParameterProcessor > (
                                                                  dispatcher );
    for ( auto v = 0 ; v < 3 ; v++ )
    {
        CREATE_VOICE_PARAMETER (
                                UnsignedInt
                              , ("Program " + String(v + 1), 0, 255)
                              , Program );
        CREATE_VOICE_PARAMETER (
                                UnsignedInt
                              , ("Attack " + String(v + 1), 0, 15)
                              , Attack );
        CREATE_VOICE_PARAMETER (
                                UnsignedInt
                              , ("Decay " + String(v + 1), 0, 15)
                              , Decay );
        CREATE_VOICE_PARAMETER (
                                UnsignedInt
                              , ("Sustain " + String(v + 1), 15, 15)
                              , Sustain );
        CREATE_VOICE_PARAMETER (
                                UnsignedInt
                              , ("Release " + String(v + 1), 0, 15)
                              , Release );
        CREATE_VOICE_PARAMETER (
                                SignedPercent
                              , ("Pulse Width " + String(v + 1), 0.0F, 4096)
                              , PulseWidth );
        CREATE_VOICE_PARAMETER (
                                Boolean
                              , ("Filter " + String(v + 1), false)
                              , FilterVoice );
        CREATE_VOICE_PARAMETER (
                                SignedPercent
                              , ("Pitch Bend " + String(v + 1), 0.0F, 16384)
                              , PitchBend );
        CREATE_VOICE_PARAMETER (
                                UnsignedPercent
                              , ("Vibrato " + String(v + 1), 0.0F, 16384)
                              , VibratoAmount );
        CREATE_VOICE_PARAMETER (
                                UnsignedPercent
                              , ("V.Speed " + String(v + 1), 1.0F, 100)
                              , VibratoSpeed );
        CREATE_VOICE_PARAMETER (
                                UnsignedInt
                              , ("V.Delay " + String(v + 1), 0, 15)
                              , VibratoDelay );
    }
    CREATE_PARAMETER (
                      UnsignedInt
                    , ("Cutoff", 0, 2047)
                    , Cutoff );
    CREATE_PARAMETER (
                      UnsignedInt
                    , ("Resonance", 0, 15)
                    , Resonance );
    CREATE_PARAMETER (
                      Boolean
                    , ("Low Pass", false)
                    , LowPass );
    CREATE_PARAMETER (
                      Boolean
                    , ("Band Pass", false)
                    , BandPass );
    CREATE_PARAMETER (
                      Boolean
                    , ("High Pass", false)
                    , HighPass );
    CREATE_PARAMETER (
                      UnsignedInt
                    , ("Volume", 15, 15)
                    , Volume );

    // ReSharper disable once CppDeclaratorNeverUsed
    const auto load = properties -> GetPatches ();  // NOLINT(clang-diagnostic-unused-variable)
}

Sidstep2AudioProcessor::~Sidstep2AudioProcessor ()
{
    core          = nullptr;
    liveMode      = nullptr;
    patchEditor   = nullptr;
    midiProcessor = nullptr;
    //exportManager = nullptr;
}


// ReSharper disable once CppConstValueFunctionReturnType
auto  // NOLINT(readability-const-return-type)
    Sidstep2AudioProcessor::getName () const -> const String { return JucePlugin_Name; }

auto
    Sidstep2AudioProcessor::acceptsMidi () const -> bool
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

auto
    Sidstep2AudioProcessor::producesMidi () const -> bool
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

auto
    Sidstep2AudioProcessor::getTailLengthSeconds () const -> double { return 0.0; }

auto
    Sidstep2AudioProcessor::getNumPrograms () -> int
{
    return 128;
}

auto
    Sidstep2AudioProcessor::getCurrentProgram () -> int { return 0; }

void
    Sidstep2AudioProcessor::setCurrentProgram (
            int index
            ) {}

// ReSharper disable once CppConstValueFunctionReturnType
auto  // NOLINT(readability-const-return-type)
    Sidstep2AudioProcessor::getProgramName (
            int index
            ) -> const String { return String (); }

void
    Sidstep2AudioProcessor::changeProgramName (
            int index
          , const String& new_name
            ) {}


void
    Sidstep2AudioProcessor::prepareToPlay (
            const double sample_rate
          , int samples_per_block
            )
{
    dispatcher -> sampleRateChangedListeners -> call (
                                                      &SampleRateChanged::onSampleRateChanged
                                                    , sample_rate );
}

void
    Sidstep2AudioProcessor::releaseResources () {}

auto
    Sidstep2AudioProcessor::isBusesLayoutSupported (
            const BusesLayout& layouts
            ) const -> bool
{
    if ( layouts . inputBuses . size () > 0 && !layouts . getMainInputChannelSet () . isDisabled () ) { return false; }
    const auto& output = layouts . getMainOutputChannelSet ();
    return output == AudioChannelSet::mono () || output == AudioChannelSet::stereo ();
}


void
    Sidstep2AudioProcessor::processBlock (
            AudioSampleBuffer& buffer
          , MidiBuffer&        midi_messages
            )
{
    ScopedNoDenormals no_denormals;

    // Hosts are allowed to provide no play head at all (offline renderers and
    // headless/scripted hosts often don't), so fall back to a stopped
    // transport instead of dereferencing a null pointer.
    AudioPlayHead::CurrentPositionInfo position_info {};
    position_info . resetToDefault ();
    if ( auto* play_head = getPlayHead () )
    {
        if ( const auto position = play_head -> getPosition () )
        {
            position_info . bpm           = position -> getBpm () . orFallback ( 0.0 );
            position_info . timeInSamples = position -> getTimeInSamples () . orFallback ( 0 );
            position_info . timeInSeconds = position -> getTimeInSeconds () . orFallback ( 0.0 );
            position_info . ppqPosition   = position -> getPpqPosition () . orFallback ( 0.0 );
            position_info . isPlaying     = position -> getIsPlaying ();
            position_info . isRecording   = position -> getIsRecording ();
            position_info . isLooping     = position -> getIsLooping ();
            if ( const auto loop = position -> getLoopPoints () )
            {
                position_info . ppqLoopStart = loop -> ppqStart;
                position_info . ppqLoopEnd   = loop -> ppqEnd;
            }
            if ( const auto sig = position -> getTimeSignature () )
            {
                position_info . timeSigNumerator   = sig -> numerator;
                position_info . timeSigDenominator = sig -> denominator;
            }
        }
    }

    buffer . clear ();
    core -> Generate (
                      midi_messages
                    , buffer
                    , position_info );

    // The SID is mono and renders into channel 0; mirror it to any other outputs.
    for ( auto channel = 1 ; channel < buffer . getNumChannels () ; ++channel )
    {
        buffer . copyFrom (
                           channel
                         , 0
                         , buffer
                         , 0
                         , 0
                         , buffer . getNumSamples () );
    }
}


auto
    Sidstep2AudioProcessor::hasEditor () const -> bool
{
    return true; 
}

auto
    Sidstep2AudioProcessor::createEditor () -> AudioProcessorEditor*
{
    auto ed = new Sidstep2AudioProcessorEditor (
                                                *this );
    ed -> addAndMakeVisible (
                             *liveMode );
    ed -> addChildComponent (
                             *patchEditor );
    //ed -> addChildComponent (
    //                         *exportManager );
    return ed;
}


void
    Sidstep2AudioProcessor::getStateInformation (
            MemoryBlock& dest_data
            )
{
    core -> GetStateInformation (
                                 dest_data );
}

void
    Sidstep2AudioProcessor::setStateInformation (
            const void* data
          , const int   size_in_bytes
            )
{
    core -> SetStateInformation (
                                 data
                               , size_in_bytes );
}

auto
    Sidstep2AudioProcessor::GetCore () const -> std::shared_ptr < SidStep2 > { return core; }


// ReSharper disable once CppInconsistentNaming
auto JUCE_CALLTYPE createPluginFilter () -> AudioProcessor* { return new Sidstep2AudioProcessor (); }
