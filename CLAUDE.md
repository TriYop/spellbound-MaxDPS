# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Spellbound Bastos is a DPF audio **effect** plugin (VST3 / CLAP / LV2) combining a dual-envelope (fast/sustain) transient designer with independent sub-harmonic and upper-harmonic generation per phase, targeting kick drum processing and general low-frequency transient shaping. It is an effect -- audio is modified in-place, not analyzed. There is no bypass parameter and no bandpass/dynamic-EQ mode -- an earlier version of this document described a "BassEnhancer" stage and bypass/bandpass controls that were never actually implemented; ignore any reference to those elsewhere, they do not exist in this codebase.

**Current state: migrated off JUCE onto DPF.** `Source/DSP/TransientShaperCore.h/.cpp` is a framework-free port of the original JUCE-dependent `TransientShaper` (replacing `juce::dsp::IIR::Filter`/`juce::AudioBuffer`/`juce::Decibels` with a hand-rolled RBJ biquad and raw channel pointers), unit-tested via CTest and verified against the JUCE original via a golden-vector capture. `Source/BastosPluginAdapter.{h,cpp}` wires all 13 host parameters and replicates the JUCE-era processBlock()'s dry/wet mix, output gain, and peak metering. `Source/BastosUI.{h,cpp}` has the ported editor (13 rotary knobs across Attack/Sustain/Global groups, 2 VU meters) plus a **new** factory-presets panel (Bastos never had one in JUCE) backed by `Source/FactoryPresets.h`'s 3 presets and `AudioPlugins/Common`'s `PresetBrowser`/`PresetSelector`/`Button`. The JUCE-era `PluginProcessor`/`PluginEditor`/original `TransientShaper` are preserved unchanged under `Source/_juce_reference/` as the porting reference. Rebranded from the legacy `YvanJanet`/`com.yvanjanet.bastos` identity to `Spellbound`/`com.spellbound.bastos`, matching every other migrated plugin. See `docs/superpowers/plans/2026-09-06-maxdps-dpf-migration.md` for the full task-by-task migration record.

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

### Configure / build / run

```bash
# Configure (first run fetches DPF + AudioPlugins/Common into build/_deps/)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
cmake --build build --parallel

# Build outputs (DPF layout, under build/bin/ -- no Standalone target:
# Bastos is an effect, not an instrument)
#   build/bin/Bastos.vst3/
#   build/bin/Bastos.clap
#   build/bin/Bastos.lv2/

# Unit tests (DSP + presets, no DPF/JUCE dependency)
ctest --test-dir build --output-on-failure

# Install plugins (Linux, dev build)
cp -r build/bin/Bastos.vst3 ~/.vst3/
cp    build/bin/Bastos.clap ~/.clap/
cp -r build/bin/Bastos.lv2  ~/.lv2/
```

### Release packaging

```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
cd build-release && cpack
```

Produces a portable `Bastos-<version>-linux-x86_64.tar.gz` with `install.sh`/`uninstall.sh` for installing VST3/CLAP/LV2 to the user's plugin directories.

### Create a Debian package

```bash
cmake -B build-deb -G Ninja -DCMAKE_BUILD_TYPE=Release -DPACKAGE_DEB=ON
cmake --build build-deb --parallel
cd build-deb && cpack
```

Produces `spellbound-bastos_<version>_<arch>.deb`, installing to
`/usr/lib/{vst3,clap,lv2}` via `dpkg`. Requires `dpkg-dev` on the build host
(provides `dpkg-shlibdeps`, which auto-derives the package's runtime
`Depends:`). Uninstall with `sudo apt remove spellbound-bastos`.

## Architecture

### Source layout

```
Source/
  DistrhoPluginInfo.h        -- DPF metadata, BastosParameters enum, BASTOS_PARAM_* ranges/defaults
  BastosPluginAdapter.h/.cpp -- DPF Plugin: parameters, dry/wet mix, output gain, peak metering
  BastosUI.h/.cpp            -- DPF UI: 13 RotaryKnobs, 2 VuMeters, presets bar
  FactoryPresets.h           -- 3 hand-authored factory presets
  DSP/
    TransientShaperCore.h/.cpp -- framework-free dual-envelope transient designer (unit-tested)
  _juce_reference/            -- pre-migration JUCE code, preserved as the porting reference
```

### Signal flow

```
run()
  |- measure input peak (MeterTransport)
  |- copy dry signal (if mix < 1.0)
  |- TransientShaperCore::process()
  |    fast envelope follower (~1-5 ms) and slow follower (~50-200 ms)
  |    transient weight t = (fast - slow) / (fast + slow); atkW = max(0,t); susW = max(0,-t)
  |    gain / sub-drive / sub-level / upper-drive / upper-level all interpolated from atkW/susW
  |    sub harmonics:   LP(x, 150 Hz) -> tanh(drive) - dry -> LP(300 Hz) -> blend
  |    upper harmonics: tanh(x * drive) - x -> blend
  |- apply output gain
  |- blend dry signal back in (if mix < 1.0)
  `- measure output peak (MeterTransport)
```

### Parameters

| Host symbol | Range | Default | Description |
|---|---|---|---|
| `atk_gain` | -12 to 12 dB | 0 | gain applied during transient attack |
| `atk_sub_count` | 1-3 | 1 | sub-harmonic drive multiplier during attack |
| `atk_sub_level` | 0-1 | 0 | sub-harmonic blend level during attack |
| `atk_upper_count` | 1-5 | 1 | upper-harmonic drive multiplier during attack |
| `atk_upper_level` | 0-1 | 0 | upper-harmonic blend level during attack |
| `sus_gain` | -12 to 12 dB | 0 | gain applied during transient sustain/decay |
| `sus_sub_count` | 1-3 | 1 | sub-harmonic drive multiplier during sustain |
| `sus_sub_level` | 0-1 | 0 | sub-harmonic blend level during sustain |
| `sus_upper_count` | 1-5 | 1 | upper-harmonic drive multiplier during sustain |
| `sus_upper_level` | 0-1 | 0 | upper-harmonic blend level during sustain |
| `speed` | 0-1 | 0 | envelope follower speed (fast<->slow time constants) |
| `output_gain` | -12 to 12 dB | 0 | output trim |
| `mix` | 0-1 | 1 | global dry/wet |

No bypass parameter exists (there never was one, even pre-migration).

### Key design constraints

- **DSP is framework-free and unit-tested independently of DPF** (`Source/DSP/TransientShaperCore.h/.cpp` + `Tests/test_transientshapercore.cpp`, CTest, verified against the original JUCE implementation via a golden-vector capture) -- `BastosPluginAdapter`/`BastosUI` are thin adapters with no DSP logic of their own.
- **In-place processing**: `run()` modifies the output buffer directly; `TransientShaperCore` owns no per-block scratch beyond its 2-channel envelope/filter state.
- **Peak meters are direct-access, not DPF parameters**: `DISTRHO_PLUGIN_WANT_DIRECT_ACCESS` + `getPluginInstancePointer()`, same idiom as every other plugin in this workspace -- clap-validator rejects host-visible, audio-reactive output parameters regardless of hints.
- **Zero latency**: `TransientShaperCore` has no delay lines; `DISTRHO_PLUGIN_WANT_LATENCY 0`.
