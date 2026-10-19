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
