# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Bastos is a JUCE audio effect plugin (VST3 / CLAP / Standalone) combining a **bass enhancer** and a **transient shaper**, targeting kick drum processing and general low-frequency content shaping. It is an effect — audio is modified in-place, not analyzed.

- **Bass enhancer**: generates sub-octave harmonics via half-wave rectification + filtering, blended into the dry signal
- **Transient shaper**: dual-envelope detector (fast − slow) that applies gain shaping during attacks and decays, optionally limited to a bandpass-filtered band (making it act as a dynamic EQ on transients)

Each stage has an independent bypass switch. Input and output level meters are displayed in the UI.

## Build Commands

### Linux prerequisites (one-time)

```bash
sudo apt install cmake ninja-build build-essential git \
    libasound2-dev libjack-jackd2-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev \
    libfreetype-dev libfontconfig1-dev \
    libglu1-mesa-dev libwebkit2gtk-4.1-dev
```

### Configure

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

First run downloads JUCE and clap-juce-extensions into `build/_deps/`.

### Build

```bash
cmake --build build --parallel          # all targets
cmake --build build --target Bastos_Standalone
cmake --build build --target Bastos_VST3
cmake --build build --target Bastos_CLAP
```

### Run standalone

```bash
./build/Bastos_artefacts/Debug/Standalone/Bastos
```

### Install plugins (Linux, dev build)

```bash
cp -r build/Bastos_artefacts/Debug/VST3/Bastos.vst3 ~/.vst3/
cp    build/Bastos_artefacts/Debug/CLAP/Bastos.clap ~/.clap/
```

### Create shippable tarball

```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
cd build-release && cpack
```

## Architecture

### Source layout

```
Source/
  PluginProcessor.h/.cpp    — AudioProcessor; APVTS definition; routes processBlock()
                              through BassEnhancer → TransientShaper; measures input/output peak levels
  PluginEditor.h/.cpp       — AudioProcessorEditor; knobs, bypass toggles, input/output meters; 30 Hz timer
  DSP/
    BassEnhancer.h/.cpp     — Sub-harmonic generator; respects bass_enabled bypass flag
    TransientShaper.h/.cpp  — Dual-envelope transient shaper with optional bandpass; respects transient_enabled flag
```

### Data flow

```
processBlock()
  ├─ measure input peak (atomic write)
  ├─ BassEnhancer::process()        [no-op if bass_enabled == false]
  │    LP filter @ cutoff → isolate lows
  │    half-wave rectify → sub-octave content
  │    bandpass generated content (keep subs only)
  │    mix back with dry signal (drive + blend)
  ├─ TransientShaper::process()     [no-op if transient_enabled == false]
  │    fast envelope follower (~1–5 ms) and slow follower (~50–200 ms)
  │    transient signal = fast − slow
  │    if bp_enabled:  bandpass dry @ bp_freq/bp_q → apply gain to band → mix back
  │    if bp_disabled: apply gain to full signal
  │    gain is smoothed per-sample (juce::SmoothedValue<float>) to prevent clicks
  └─ measure output peak (atomic write)
```

### Parameters (APVTS)

| ID | Range | Description |
|----|-------|-------------|
| `bass_enabled` | bool | bypass the bass enhancer |
| `drive` | 0–24 dB | harmonic generation drive |
| `cutoff` | 60–200 Hz | LP cutoff for bass isolation |
| `blend` | 0–100 % | wet/dry mix of generated sub content |
| `transient_enabled` | bool | bypass the transient shaper |
| `attack` | −12 to +12 dB | gain applied during transient onset |
| `sustain` | −12 to +12 dB | gain applied during transient decay |
| `speed` | fast/med/slow | envelope time constants |
| `bp_enabled` | bool | engage bandpass filter for dynamic EQ mode |
| `bp_freq` | 60–500 Hz | bandpass center frequency |
| `bp_q` | 0.5–8 | bandpass Q |
| `output_gain` | −12 to +12 dB | output trim |
| `mix` | 0–100 % | global dry/wet |

### Key design constraints

- **In-place processing**: processBlock() modifies the buffer directly; DSP classes allocate their own internal scratch buffers in `prepare()`
- **Parameter state**: APVTS owns all parameters; `getStateInformation()` / `setStateInformation()` serialize via XML ValueTree
- **Level meters**: processor writes peak values as `std::atomic<float>` each block; editor's 30 Hz timer reads them (same atomic pattern as MixAdvice's AnalysisResult)
- **Bypass**: each DSP class returns early in `process()` when its enabled flag is false; flag read from APVTS atomically
- **Click prevention**: gain modulation in TransientShaper is smoothed per-sample using `juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>` (ramp ~1 ms, initialized in `prepare()`)
- **Envelope coefficients**: compute alpha from sample rate in `prepare()`, same pattern as MixAdvice's `rmsAlpha_`
- **Half-wave rectification**: `x = (x > 0.f) ? x : 0.f` on the LP-filtered signal; result is LP-filtered again to extract the sub-octave fundamental before mixing
