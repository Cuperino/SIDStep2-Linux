# SIDStep2
A VSTi that emulates a SID chip (using reSid) and then, when you are done composing, it can capture and export your song for use on a real C64 in games and demos.

Download the latest [here](https://github.com/deathybrs/SIDStep2/releases/tag/2022.07.28.02.32)

We also have a [discord server](https://discord.gg/FRJTuCbWeD).

## Building on Linux (Ubuntu 26.04)

Install the build dependencies:

```
sudo apt install build-essential cmake ninja-build pkg-config git \
  libasound2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev
# optional: JACK support in the standalone app, headless display, KDE file dialogs
sudo apt install libjack-jackd2-dev xvfb kdialog
```

Fetch the two source dependencies into `Requirements/` (gitignored):

```
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git Requirements/JUCE
curl -L https://www.zimmers.net/anonftp/pub/cbm/crossplatform/emulators/resid/resid-0.16.tar.gz | tar -xz -C Requirements
```

Build:

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This produces `build/SIDStep2_artefacts/Release/{VST3/SIDStep2.vst3,LV2/SIDStep2.lv2,Standalone/SIDStep2}`.
Copy the VST3 bundle to `~/.vst3` and the LV2 bundle to `~/.lv2` (or configure with
`-DSIDSTEP2_COPY_AFTER_BUILD=ON`). VST2 is only built if you pass `-DSIDSTEP2_VST2_SDK=<path>`.

The plugin binaries are linked against old glibc symbol versions (`Linux/glibc_compat.c`), so they
also load in sandboxed hosts with an older runtime, such as the Flatpak builds of LMMS (via its bundled
Carla), Ardour or Carla.

### Rendering without a DAW

`sidstep2-render` is a small command-line host that plays a MIDI file through the plugin and
writes a WAV, with no display or audio device needed. MIDI channels 1-3 drive SID voices 1-3.

```
R=build/sidstep2-render_artefacts/Release/sidstep2-render
P=build/SIDStep2_artefacts/Release/VST3/SIDStep2.vst3
$R --plugin $P --list-params
$R --plugin $P --midi song.mid --out song.wav --param "Low Pass=1" --param "Filter 1=1" --param "Cutoff=0.2"
$R --plugin $P --test --out test.wav --state-out my.state    # built-in riff, save state
$R --plugin $P --midi song.mid --state-in my.state --out song2.wav
```
