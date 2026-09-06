# MaxDPS (Bastos) DPF Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate MaxDPS (product name "Bastos", a bass enhancer + transient shaper effect plugin, currently JUCE VST3/CLAP/Standalone via clap-juce-extensions) off JUCE onto DPF (DISTRHO Plugin Framework, ISC-licensed), reusing the exact skeleton/UI/presets pattern Hex, Pugilist, Tank, and Outflank already proved — **and, unlike those four, first extract `TransientShaper` (the plugin's only DSP class) out of JUCE into a plain-C++ core**, since it currently embeds `juce::dsp::ProcessSpec`, `juce::dsp::IIR::Filter<float>`, `juce::AudioBuffer<float>`, and `juce::Decibels` directly — the only one of the five migrated-so-far plugins whose DSP was not already framework-free. Also adds a factory-presets panel Bastos never had, matching the Tank precedent.

**Architecture:** Two pieces of work, kept deliberately separate: (1) a JUCE-free `TransientShaperCore` (`Source/DSP/TransientShaperCore.h/.cpp`) that ports the existing dual-envelope/sub-harmonic/upper-harmonic algorithm sample-for-sample, replacing `juce::dsp::IIR::Filter<float>` with a hand-rolled RBJ-cookbook low-pass biquad (`BiquadLowpass`, same formula JUCE's `IIR::Coefficients<float>::makeLowPass(sampleRate, frequency)` 2-argument overload uses internally: `Q = 1/√2`) and replacing `juce::AudioBuffer<float>&`/`juce::Decibels::decibelsToGain` with raw `float*` channel pointers and a local `dbToGain()` helper; verified against the JUCE original via a one-off golden-vector capture (Task 1) before the JUCE code is retired. (2) the now-familiar DPF port: a thin `BastosPluginAdapter : public Plugin` that owns `TransientShaperCore` plus the block-level dry/wet mix, output-gain, and input/output peak-metering logic `PluginProcessor::processBlock()` used to do directly (that block-level logic is NOT JUCE-dependent today — `juce::Decibels::gainToDecibels`/`decibelsToGain` and buffer copies are the only JUCE touch-points, both trivially replaced inline in the adapter), and a `BastosUI : public UI` driving `AudioPlugins/Common`'s DGL widgets (13 `RotaryKnob`s across Attack/Sustain/Global groups, 2 `VuMeter`s for IN/OUT, a new `PresetSelector`/`Button` bar). The JUCE-era `PluginProcessor`/`PluginEditor`/the original JUCE-dependent `TransientShaper` move unchanged to `Source/_juce_reference/` as the porting reference, exactly as Hex/Pugilist/Tank/Outflank did.

**Tech Stack:** C++20, CMake + Ninja, DPF (DISTRHO Plugin Framework, pinned commit, ISC), `AudioPlugins/Common` v0.3.0 (`AudioPluginsCommon::io`/`hui_dgl`/`presets`), pugixml (via Common), CTest for framework-free unit tests (`test_runner.h` CHECK/CHECK_MSG/TEST_SUMMARY convention, copied verbatim from Hex/Tank/Outflank's `Tests/test_runner.h`).

**Spec:** No prior Common-repo design spec covers MaxDPS specifically (unlike Outflank/Tank's `2026-09-05-phase2-outflank-tank-dpf-migration-design.md`) — this plan is Phase 3 of the workspace-wide JUCE→DPF migration and is self-contained, following the pattern documented in Tank's and Outflank's own plan files (`AudioPlugins/Tank/docs/superpowers/plans/2026-09-05-tank-dpf-migration.md`, `AudioPlugins/Outflank/docs/superpowers/plans/2026-09-05-outflank-dpf-migration.md`) and Hex's merged `CLAUDE.md`/`Source/` as the reference implementation to copy patterns from verbatim (DPF patches, CMake ordering, direct-access meter idiom, presets panel).

## Global Constraints

- **`MaxDPS/CLAUDE.md` is stale and must not be trusted for parameter/architecture facts.** It describes a `bass_enabled`/`drive`/`cutoff`/`blend` "BassEnhancer" stage, `transient_enabled`/`bp_enabled`/`bp_freq`/`bp_q` bypass and bandpass controls, and `juce::SmoothedValue`-based gain smoothing — **none of this exists in the actual source.** `Source/DSP/` contains only `TransientShaper.h/.cpp`; there is no `BassEnhancer.h/.cpp` anywhere in the repo (confirmed by directory listing), no bypass parameter of any kind exists in `PluginProcessor.cpp::createParameterLayout()`, and there is no bandpass filter or `SmoothedValue` anywhere in `TransientShaper.cpp`. The real, shipped parameter set (verified by reading `Source/PluginProcessor.cpp` and `Source/PluginEditor.cpp` directly) is 13 parameters: `atk_gain`, `atk_sub_count`, `atk_sub_level`, `atk_upper_count`, `atk_upper_level`, `sus_gain`, `sus_sub_count`, `sus_sub_level`, `sus_upper_count`, `sus_upper_level`, `speed`, `output_gain`, `mix` — driving a single dual-envelope (fast/slow) transient designer with independent sub-harmonic and upper-harmonic generation per attack/sustain phase. This plan migrates **the code as shipped**, not the aspirational doc; Task 8 rewrites `CLAUDE.md` to match reality and explicitly calls out that the old doc was wrong so nobody re-introduces the fictional bypass/bandpass controls later.
- DPF has no tagged releases; pin the exact commit SHA `4238e1c7f0351bbe488d79f0899c540543ac7583` (same commit Hex/Pugilist/Tank/Outflank use), applying `dpf-clap-state-chunked-read.patch` and `dpf-clap-activate-latency.patch` via `cmake/apply_patch.cmake` — copy all three files verbatim from Hex's `main` branch (`/home/yvan/Projects/AudioPlugins/Hex/cmake/patches/`, already merged there, no need to fetch a stage-0 branch), not retyped from scratch. Bastos reports **no latency** (`TransientShaperCore` has no delay lines — only per-sample IIR/envelope math, exactly like Outflank's zero-latency `CrossoverMS`), so Tank's *third* patch (`dpf-clap-latency-deactivate-report.patch`, needed only because Tank's lookahead parameter changes real latency at runtime) is **not required** here — do not copy it.
- `CMAKE_POSITION_INDEPENDENT_CODE ON` must be set *before* `FetchContent_MakeAvailable(AudioPluginsCommon)`, and the `AudioPluginsCommon` `FetchContent_Declare`/`MakeAvailable` pair must come *after* `dpf_add_plugin(Bastos ...)` (DPF's DGL target is created lazily inside that call) — both ordering constraints are load-bearing, carried over verbatim from Hex/Tank/Outflank's `CMakeLists.txt`.
- `AudioPlugins/Common` is confirmed tagged `v0.3.0` on `https://github.com/TriYop/spellbound-common.git` as of 2026-09-06 (`git ls-remote --tags` shows it) — unlike Tank's plan, this is not a blocker to re-verify as risky; Task 2 Step 1 still re-checks it defensively before pinning, since tags are mutable in principle and this plan may execute later than written.
- **CI needs a `COMMON_REPO_TOKEN` secret to clone the private `spellbound-common` repo — this was missed on both Tank's and Outflank's PRs and caused a day of red CI on each.** Task 2 (the DPF skeleton task, which first adds CI) must include setting this secret on the actual MaxDPS GitHub repo *and* verifying it with `gh secret list` before Task 7's final CI-verification task, not discovered after the fact. Verify the repo name from `git remote -v` first — do **not** assume it matches the plugin's product name or the local directory name (see Task 2 Step 2: MaxDPS's `origin` remote is `git@github.com:TriYop/spellbound-MaxDPS.git`, capital `MaxDPS`, not `spellbound-bastos`).
- **Branding unification, a deliberate decision, not scope creep:** the current JUCE build uses legacy branding (`COMPANY_NAME "YvanJanet"`, `PLUGIN_MANUFACTURER_CODE Yjnt`, `BUNDLE_ID com.yvanjanet.bastos`) inconsistent with every other migrated plugin's `Spellbound`/`Spbd`/`com.spellbound.<name>` branding (Hex, Tank, Outflank). This migration rebrands Bastos to match: `DISTRHO_PLUGIN_BRAND "Spellbound"`, `DISTRHO_PLUGIN_BRAND_ID Spbd`, `DISTRHO_PLUGIN_URI https://spellbound.audio/plugins/bastos`, `DISTRHO_PLUGIN_CLAP_ID com.spellbound.bastos`. The 4-char `DISTRHO_PLUGIN_UNIQUE_ID` reuses the existing `PLUGIN_CODE Bsts` unchanged (it's already unique and short, no reason to invent a new one). This is a user-facing bundle ID change — acceptable pre-1.0 with no other consumers, same tradeoff Tank/Outflank accepted implicitly by rebranding under Spellbound from day one.
- Windows/macOS CI legs stay `continue-on-error: true`; only the Linux leg gates the workflow (Win/Mac/`.deb` are Phase 6+ per the workspace roadmap, not an exit criterion here).
- `AudioPluginsCommon::hui_dgl` and `AudioPluginsCommon::presets` link only into the `Bastos-ui` static lib DPF creates (never into `Bastos`/`Bastos-dsp` or the LV2 binary).
- "Ticket per task" workspace convention: file one GitHub issue per validator finding discovered in Task 6, reference the issue number in the fixing commit.
- Standing workspace expectation (clean code, hexagonal/DDD, TDD) applies throughout: DSP stays framework-free and unit-tested; DPF/DGL adapter code is a thin shim with no business logic of its own. Task 1's DSP extraction is TDD in the strict sense: the test is written and run failing (no header exists) *before* `TransientShaperCore` is written.
- No copyleft license contamination: DPF is ISC-licensed; do not add any GPL/LGPL dependency.
- Bastos is stereo-in/stereo-out only, no sidechain, no MIDI, no bypass parameter (none exists today — do not add one; see the stale-`CLAUDE.md` note above), and reports zero latency.

---

## Task 1: Extract `TransientShaper` into a JUCE-free `TransientShaperCore` (TDD, golden-vector verified)

This is the task that makes MaxDPS different from every prior migration: `Source/DSP/TransientShaper.h/.cpp` currently `#include <juce_dsp/juce_dsp.h>` and `<juce_audio_basics/juce_audio_basics.h>` directly (confirmed by reading both files) and cannot be reused unchanged the way Tank's `BandpassFilter`/`RmsDetector`/`DuckingEnvelope` or Outflank's `CrossoverMS` were. This task runs **before** Task 2's CMake rewrite, while the existing JUCE build still configures/builds, so a temporary JUCE-linked "golden vector" generator can capture the original's real output for regression comparison.

**Files:**
- Create (temporary, not committed as a build target — see Step 5): `Tests/generate_golden_vectors.cpp`
- Create: `Source/DSP/TransientShaperCore.h`
- Create: `Source/DSP/TransientShaperCore.cpp`
- Create: `Tests/test_transientshapercore.cpp`
- Create: `Tests/test_runner.h` (copy verbatim from Hex/Tank/Outflank)
- Move (Step 7): `Source/DSP/TransientShaper.h` → `Source/_juce_reference/DSP/TransientShaper.h`
- Move (Step 7): `Source/DSP/TransientShaper.cpp` → `Source/_juce_reference/DSP/TransientShaper.cpp`
- Modify (temporarily, then reverted): `CMakeLists.txt` (still the JUCE-era file at this point in the plan)

**Interfaces:**
- Produces: `class TransientShaperCore` (`Source/DSP/TransientShaperCore.h`) with `prepare(double sampleRate)`, `reset()`, and `process(float* const* channels, int numChannels, int numSamples, float atkGainDb, int atkSubCount, float atkSubLevel, int atkUpperCount, float atkUpperLevel, float susGainDb, int susSubCount, float susSubLevel, int susUpperCount, float susUpperLevel, float speedFrac, bool enabled) noexcept` — same parameter shape as the JUCE original's `process()`, minus the `juce::AudioBuffer<float>&` (replaced by raw channel pointers + count), consumed by `BastosPluginAdapter::run()` in Task 3.
- Produces: `class BiquadLowpass` (same header), a framework-free RBJ-cookbook low-pass biquad with `setCutoff(double sampleRate, float frequencyHz) noexcept`, `float processSample(float x) noexcept`, `reset() noexcept` — a from-scratch replacement for `juce::dsp::IIR::Filter<float>` + `juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, frequency)` (the 2-argument overload, which JUCE implements internally as a `Q = 1/√2` Butterworth-response biquad, not a first-order filter — verified against JUCE's own documented behavior for that overload).

- [ ] **Step 1: Copy `test_runner.h` and write the failing test's unconditional invariants**

```bash
cd /home/yvan/Projects/AudioPlugins/MaxDPS   # or the Task 2 worktree once one exists — this task runs first, directly on a clean main checkout is fine since nothing here touches CMakeLists.txt permanently
mkdir -p Tests
cp /home/yvan/Projects/AudioPlugins/Hex/Tests/test_runner.h Tests/test_runner.h
```

Write `Tests/test_transientshapercore.cpp`:

```cpp
#include "test_runner.h"
#include "../Source/DSP/TransientShaperCore.h"

#include <array>
#include <cmath>

int main()
{
    // Invariant 1: silence in -> silence out, for ANY parameter combination.
    // Hand-verifiable without running anything: with x=0 every sample, rect=0,
    // both envelopes decay toward (and, starting from reset() state, stay at)
    // 0, so envSum=1e-7 (the anti-div-by-zero epsilon), t=0, atkW=susW=0,
    // gain=1, bass=0 -> subSat=0 -> subHarm=0, upperSat=0 -> upperHarm=0.
    // Output = 1*0 + 0 + 0 = 0, independent of drive/level/gain parameters.
    {
        TransientShaperCore core;
        core.prepare(48000.0);

        std::array<float, 64> left{};
        std::array<float, 64> right{};
        float* channels[2] = { left.data(), right.data() };

        core.process(channels, 2, 64,
                      12.0f, 3, 1.0f, 5, 1.0f,   // attack: max gain/drive/level
                      -12.0f, 3, 1.0f, 5, 1.0f,  // sustain: min gain, max drive/level
                      0.5f, true);

        for (float v : left)  CHECK(v == 0.0f);
        for (float v : right) CHECK(v == 0.0f);
    }

    // Invariant 2: enabled=false is a hard passthrough (the JUCE original's
    // `if (!enabled) return;` at the top of process()) -- any non-silent
    // input must come back byte-for-byte unmodified.
    {
        TransientShaperCore core;
        core.prepare(48000.0);

        std::array<float, 8> left  { 0.1f, -0.2f, 0.3f, -0.4f, 0.5f, -0.6f, 0.7f, -0.8f };
        const std::array<float, 8> leftOriginal = left;
        float* channels[1] = { left.data() };

        core.process(channels, 1, 8,
                      6.0f, 2, 0.5f, 3, 0.5f,
                      -6.0f, 2, 0.5f, 3, 0.5f,
                      0.2f, false);

        for (size_t i = 0; i < left.size(); ++i)
            CHECK(left[i] == leftOriginal[i]);
    }

    // Invariant 3 (golden vectors): filled in by Step 5 below, comparing
    // TransientShaperCore's output against the real JUCE-era TransientShaper's
    // captured output for a non-trivial (non-silent, enabled) input -- this
    // is the actual behavior-preservation check, and it can't be hand-derived
    // like Invariants 1/2 above.

    TEST_SUMMARY();
    return 0;
}
```

- [ ] **Step 2: Verify the test fails to compile (no header yet)**

```bash
g++ -std=c++20 -I Source -I Tests -c Tests/test_transientshapercore.cpp -o /tmp/test_tsc.o
```

Expected: FAIL — `Source/DSP/TransientShaperCore.h` doesn't exist yet.

- [ ] **Step 3: Write `Source/DSP/TransientShaperCore.h`**

```cpp
#pragma once

#include <array>
#include <cmath>

// Framework-free RBJ-cookbook low-pass biquad, replacing
// juce::dsp::IIR::Filter<float> + juce::dsp::IIR::Coefficients<float>::
// makeLowPass(sampleRate, frequency) (the 2-argument overload) exactly:
// that overload is documented to construct a Q = 1/sqrt(2) (Butterworth-
// response) biquad, NOT a first-order one-pole filter, so this class must
// be a real 2-pole biquad to reproduce the same transfer function.
//
// Direct Form I: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
// (coefficients pre-normalized so a0 == 1). Mathematically identical to
// whatever internal structure JUCE's IIR::Filter uses for the same
// coefficients -- floating-point rounding aside, which Tests/test_
// transientshapercore.cpp's golden-vector comparison tolerates via an
// epsilon, not exact equality.
class BiquadLowpass
{
public:
    void setCutoff(double sampleRate, float frequencyHz) noexcept
    {
        constexpr float kQ = 0.70710678f; // 1/sqrt(2), JUCE's makeLowPass(sr, f) default
        const double w0 = 2.0 * M_PI * static_cast<double>(frequencyHz) / sampleRate;
        const double cosw0 = std::cos(w0);
        const double sinw0 = std::sin(w0);
        const double alpha = sinw0 / (2.0 * kQ);

        const double a0 = 1.0 + alpha;
        b0_ = static_cast<float>(((1.0 - cosw0) / 2.0) / a0);
        b1_ = static_cast<float>((1.0 - cosw0) / a0);
        b2_ = b0_;
        a1_ = static_cast<float>((-2.0 * cosw0) / a0);
        a2_ = static_cast<float>((1.0 - alpha) / a0);
    }

    void reset() noexcept { x1_ = x2_ = y1_ = y2_ = 0.f; }

    float processSample(float x) noexcept
    {
        const float y = b0_ * x + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
        x2_ = x1_; x1_ = x;
        y2_ = y1_; y1_ = y;
        return y;
    }

private:
    float b0_ = 1.f, b1_ = 0.f, b2_ = 0.f, a1_ = 0.f, a2_ = 0.f;
    float x1_ = 0.f, x2_ = 0.f, y1_ = 0.f, y2_ = 0.f;
};

// JUCE-free port of Source/_juce_reference/DSP/TransientShaper.h -- see that
// file's header comment for the full algorithm description (dual envelope
// follower, sub/upper harmonic generation per attack/sustain phase). This
// class is byte-for-byte the same algorithm; only the framework touch points
// changed (see the class comment in TransientShaperCore.cpp).
class TransientShaperCore
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // channels[i] is numSamples long, in-place. numChannels is 1 or 2 (mirrors
    // the JUCE original's `std::min(buffer.getNumChannels(), 2)` clamp).
    void process(float* const* channels, int numChannels, int numSamples,
                 float atkGainDb, int atkSubCount, float atkSubLevel,
                 int atkUpperCount, float atkUpperLevel,
                 float susGainDb, int susSubCount, float susSubLevel,
                 int susUpperCount, float susUpperLevel,
                 float speedFrac, bool enabled) noexcept;

private:
    struct ChannelState
    {
        float envFast = 0.f;
        float envSlow = 0.f;
        BiquadLowpass subInputLp;   // LP at 150 Hz -- isolates bass for sub generation
        BiquadLowpass subOutputLp;  // LP at 300 Hz -- keeps sub harmonics in bass range
    };

    std::array<ChannelState, 2> ch_;

    double sampleRate_ = 44100.0;
    float  alphaFast_  = 0.f;
    float  alphaSlow_  = 0.f;
    float  lastSpeed_  = -1.f;

    void updateEnvCoeffs(float speedFrac) noexcept;

    static constexpr float kSubInputCutoffHz  = 150.f;
    static constexpr float kSubOutputCutoffHz = 300.f;

    static float dbToGain(float db) noexcept { return std::pow(10.f, db / 20.f); }
};
```

- [ ] **Step 4: Write `Source/DSP/TransientShaperCore.cpp`**

```cpp
#include "TransientShaperCore.h"

#include <algorithm>

void TransientShaperCore::prepare(double sampleRate) noexcept
{
    sampleRate_ = sampleRate;
    lastSpeed_  = -1.f;

    for (auto& c : ch_)
    {
        c.subInputLp.setCutoff(sampleRate, kSubInputCutoffHz);
        c.subOutputLp.setCutoff(sampleRate, kSubOutputCutoffHz);
    }

    reset();
}

void TransientShaperCore::reset() noexcept
{
    for (auto& c : ch_)
    {
        c.envFast = 0.f;
        c.envSlow = 0.f;
        c.subInputLp.reset();
        c.subOutputLp.reset();
    }
}

void TransientShaperCore::updateEnvCoeffs(float speedFrac) noexcept
{
    if (std::abs(speedFrac - lastSpeed_) < 1e-4f) return;
    lastSpeed_ = speedFrac;

    // fast: 1-5 ms; slow: 50-200 ms -- unchanged from the JUCE original.
    const float sr     = static_cast<float>(sampleRate_);
    const float fastMs = 1.f + speedFrac * 4.f;
    const float slowMs = 50.f + speedFrac * 150.f;
    alphaFast_ = std::exp(-1000.f / (fastMs * sr));
    alphaSlow_ = std::exp(-1000.f / (slowMs * sr));
}

void TransientShaperCore::process(float* const* channels, int numChannels, int numSamples,
                                   float atkGainDb, int atkSubCount, float atkSubLevel,
                                   int atkUpperCount, float atkUpperLevel,
                                   float susGainDb, int susSubCount, float susSubLevel,
                                   int susUpperCount, float susUpperLevel,
                                   float speedFrac, bool enabled) noexcept
{
    if (!enabled) return;

    const int nChannels = std::min(numChannels, 2);

    updateEnvCoeffs(speedFrac);

    const float atkGainLin    = dbToGain(atkGainDb);
    const float susGainLin    = dbToGain(susGainDb);
    const float atkSubDrive   = 3.f * static_cast<float>(atkSubCount);
    const float susSubDrive   = 3.f * static_cast<float>(susSubCount);
    const float atkUpperDrive = 2.f * static_cast<float>(atkUpperCount);
    const float susUpperDrive = 2.f * static_cast<float>(susUpperCount);

    for (int ch = 0; ch < nChannels; ++ch)
    {
        auto& state = ch_[static_cast<size_t>(ch)];
        float* buf  = channels[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float x    = buf[i];
            const float rect = std::abs(x);

            state.envFast = alphaFast_ * state.envFast + (1.f - alphaFast_) * rect;
            state.envSlow = alphaSlow_ * state.envSlow + (1.f - alphaSlow_) * rect;

            const float envSum = state.envFast + state.envSlow + 1e-7f;
            const float t      = (state.envFast - state.envSlow) / envSum;
            const float atkW   = std::max(0.f, t);
            const float susW   = std::max(0.f, -t);

            const float gain       = 1.f + atkW * (atkGainLin - 1.f) + susW * (susGainLin - 1.f);
            const float subDrive   = atkW * atkSubDrive   + susW * susSubDrive;
            const float subLevel   = atkW * atkSubLevel   + susW * susSubLevel;
            const float upperDrive = atkW * atkUpperDrive + susW * susUpperDrive;
            const float upperLevel = atkW * atkUpperLevel + susW * susUpperLevel;

            const float bass    = state.subInputLp.processSample(x);
            const float subSat  = subDrive > 0.01f ? std::tanh(bass * subDrive) : bass;
            const float subHarm = state.subOutputLp.processSample(subSat - bass);

            const float upperSat  = upperDrive > 0.01f ? std::tanh(x * upperDrive) : x;
            const float upperHarm = upperSat - x;

            buf[i] = gain * x + subLevel * subHarm + upperLevel * upperHarm;
        }
    }
}
```

- [ ] **Step 5: Capture golden vectors from the real JUCE `TransientShaper` and add them as Invariant 3**

While `Source/DSP/TransientShaper.h/.cpp` still exist at their original path and the JUCE build still configures, temporarily add a generator that links against the same JUCE modules the existing `CMakeLists.txt` already fetches:

```cpp
// Tests/generate_golden_vectors.cpp -- TEMPORARY, not committed as a
// permanent build target. Deleted in Step 7 once its output is captured
// into Tests/test_transientshapercore.cpp's Invariant 3.
#include "../Source/DSP/TransientShaper.h"
#include <juce_dsp/juce_dsp.h>
#include <cstdio>

int main()
{
    TransientShaper shaper;
    juce::dsp::ProcessSpec spec { 48000.0, 32, 2 };
    shaper.prepare (spec);

    // A short burst: silence, then an 8-sample unit-amplitude square wave,
    // then decay -- enough to actually drive both envelope followers away
    // from zero and exercise the sub/upper harmonic paths.
    juce::AudioBuffer<float> buf (2, 32);
    buf.clear();
    for (int i = 4; i < 12; ++i) { buf.setSample (0, i, (i % 2 == 0) ? 1.0f : -1.0f);
                                    buf.setSample (1, i, (i % 2 == 0) ? 1.0f : -1.0f); }

    shaper.process (buf,
                     /*atkGainDb*/ 6.0f, /*atkSubCount*/ 2, /*atkSubLevel*/ 0.5f,
                     /*atkUpperCount*/ 3, /*atkUpperLevel*/ 0.5f,
                     /*susGainDb*/ -4.0f, /*susSubCount*/ 1, /*susSubLevel*/ 0.3f,
                     /*susUpperCount*/ 2, /*susUpperLevel*/ 0.3f,
                     /*speedFrac*/ 0.3f, /*enabled*/ true);

    std::printf ("static constexpr float kGoldenLeft[32] = {\n");
    for (int i = 0; i < 32; ++i)
        std::printf ("    %.9ff,%s", buf.getSample (0, i), (i % 4 == 3) ? "\n" : " ");
    std::printf ("};\n");
    return 0;
}
```

Build and run it once, using the still-JUCE `CMakeLists.txt`:

```bash
# Append a throwaway target (do NOT commit this CMakeLists.txt edit):
cat >> CMakeLists.txt <<'EOF'
add_executable(generate_golden_vectors Tests/generate_golden_vectors.cpp Source/DSP/TransientShaper.cpp)
target_include_directories(generate_golden_vectors PRIVATE Source/)
target_link_libraries(generate_golden_vectors PRIVATE juce::juce_dsp juce::juce_audio_basics)
target_compile_features(generate_golden_vectors PRIVATE cxx_std_20)
EOF

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target generate_golden_vectors
./build/generate_golden_vectors
```

Copy the printed `kGoldenLeft[32]` array's real, actual output into `Tests/test_transientshapercore.cpp` as Invariant 3, driving `TransientShaperCore` with the **exact same** input burst and parameters and comparing element-by-element within a small epsilon (floating-point rounding differences between JUCE's internal biquad structure and `BiquadLowpass`'s Direct Form I are expected, exact bit-equality is not the bar):

```cpp
    // Invariant 3: golden-vector regression against the real JUCE-era
    // TransientShaper's output for a non-trivial input, captured via
    // Tests/generate_golden_vectors.cpp (see Task 1 Step 5 in the migration
    // plan for how these numbers were obtained -- they are NOT hand-derived).
    {
        static constexpr float kGoldenLeft[32] = {
            // <<< PASTE THE REAL PRINTED OUTPUT FROM THIS STEP'S RUN HERE >>>
        };

        TransientShaperCore core;
        core.prepare(48000.0);

        std::array<float, 32> left{};
        std::array<float, 32> right{};
        for (int i = 4; i < 12; ++i)
        {
            left[static_cast<size_t>(i)]  = (i % 2 == 0) ? 1.0f : -1.0f;
            right[static_cast<size_t>(i)] = (i % 2 == 0) ? 1.0f : -1.0f;
        }
        float* channels[2] = { left.data(), right.data() };

        core.process(channels, 2, 32,
                      6.0f, 2, 0.5f, 3, 0.5f,
                      -4.0f, 1, 0.3f, 2, 0.3f,
                      0.3f, true);

        for (size_t i = 0; i < left.size(); ++i)
            CHECK_MSG(std::abs(left[i] - kGoldenLeft[i]) < 1e-4f, "sample mismatch vs. JUCE golden vector");
    }
```

Do not skip pasting real numbers here — a golden-vector test with the array left empty or filled with guessed values defeats the entire point of this step.

- [ ] **Step 6: Remove the temporary generator target and revert the `CMakeLists.txt` edit**

```bash
git checkout -- CMakeLists.txt   # discards Step 5's temporary add_executable block
rm Tests/generate_golden_vectors.cpp
```

(`Tests/generate_golden_vectors.cpp` is deliberately not preserved — its only purpose was producing the numbers now hardcoded in `Tests/test_transientshapercore.cpp`; if it's ever needed again, Step 5's snippet above is the reference for recreating it.)

- [ ] **Step 7: Move the JUCE-era `TransientShaper` to `Source/_juce_reference/DSP/`, build the new test standalone**

```bash
mkdir -p Source/_juce_reference/DSP
git mv Source/DSP/TransientShaper.h   Source/_juce_reference/DSP/TransientShaper.h
git mv Source/DSP/TransientShaper.cpp Source/_juce_reference/DSP/TransientShaper.cpp

g++ -std=c++20 -O2 -I Source -I Tests Tests/test_transientshapercore.cpp Source/DSP/TransientShaperCore.cpp -o /tmp/test_tsc
/tmp/test_tsc
```

Expected: all CHECKs pass (2 silence samples × 2 channels × 64 samples for Invariant 1, 8 for Invariant 2, 32 for Invariant 3's golden-vector comparison) — `N passed, 0 failed`.

- [ ] **Step 8: Commit**

```bash
git add Source/DSP/TransientShaperCore.h Source/DSP/TransientShaperCore.cpp \
        Source/_juce_reference/DSP/TransientShaper.h Source/_juce_reference/DSP/TransientShaper.cpp \
        Tests/test_runner.h Tests/test_transientshapercore.cpp
git commit -m "$(cat <<'EOF'
Extract TransientShaper into a JUCE-free TransientShaperCore

Ports the dual-envelope sub/upper-harmonic transient designer to plain
C++20: juce::dsp::IIR::Filter<float> becomes a hand-rolled RBJ-cookbook
BiquadLowpass (same Q=1/sqrt(2) response as JUCE's makeLowPass(sr, f)
2-arg overload), juce::AudioBuffer<float>& becomes raw channel pointers,
juce::Decibels::decibelsToGain becomes a local dbToGain(). Verified
against the original via a golden-vector capture (temporary generator,
not committed) plus two hand-derivable invariants (silence-in/silence-
out, enabled=false passthrough). Old JUCE-dependent TransientShaper
preserved under Source/_juce_reference/DSP/ as the porting reference.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: DPF skeleton — isolated worktree, CMake rewrite, plugin metadata, JUCE-reference move, stub adapter/UI, build-only CI

**Files:**
- Create (via worktree): `.claude/worktrees/dpf-stage0/` on branch `worktree-dpf-stage0`
- Create: `cmake/apply_patch.cmake`
- Create: `cmake/patches/dpf-clap-state-chunked-read.patch`
- Create: `cmake/patches/dpf-clap-activate-latency.patch`
- Create: `Source/DistrhoPluginInfo.h`
- Create: `Source/BastosPluginAdapter.h` (stub)
- Create: `Source/BastosPluginAdapter.cpp` (stub)
- Create: `Source/BastosUI.h` (stub)
- Create: `Source/BastosUI.cpp` (stub)
- Move: `Source/PluginProcessor.h` → `Source/_juce_reference/PluginProcessor.h`
- Move: `Source/PluginProcessor.cpp` → `Source/_juce_reference/PluginProcessor.cpp`
- Move: `Source/PluginEditor.h` → `Source/_juce_reference/PluginEditor.h`
- Move: `Source/PluginEditor.cpp` → `Source/_juce_reference/PluginEditor.cpp`
- Modify: `CMakeLists.txt` (full rewrite)
- Modify: `scripts/install.sh`, `scripts/uninstall.sh`
- Create: `.github/workflows/ci.yml`
- Unchanged: `Source/DSP/TransientShaperCore.h/.cpp`, `Tests/*.cpp` (Task 1's output)

**Interfaces:**
- Produces: `BastosPluginAdapter` (stub `Plugin` subclass) and `BastosUI` (stub `UI` subclass) — Task 3 fills in DSP wiring, Task 4 fills in widgets.
- Produces: `Source/DistrhoPluginInfo.h`'s `BastosParameters` enum and `BASTOS_PARAM_*_MIN/MAX/DEFAULT` macros for all 13 parameters — every later task's parameter code uses these names verbatim.

- [ ] **Step 1: Confirm MaxDPS's current git state and remote, then create the isolated worktree**

```bash
cd /home/yvan/Projects/AudioPlugins/MaxDPS
git status --short
git remote -v
```

Expected `origin`: `git@github.com:TriYop/spellbound-MaxDPS.git` (confirmed capitalization — do not assume `spellbound-bastos` or `spellbound-maxdps`; every `gh` command in this plan targets `TriYop/spellbound-MaxDPS` exactly as shown here). Then, in a fresh session, invoke `superpowers:using-git-worktrees`:

```bash
git fetch origin
git worktree add .claude/worktrees/dpf-stage0 -b worktree-dpf-stage0 origin/main
cd .claude/worktrees/dpf-stage0
```

All remaining steps in this plan run from inside that worktree directory.

- [ ] **Step 2: Set the `COMMON_REPO_TOKEN` secret now, not after discovering CI red**

```bash
gh secret list --repo TriYop/spellbound-MaxDPS
```

If `COMMON_REPO_TOKEN` is not already listed, set it using the same fine-grained PAT already used for Hex/Pugilist/Tank/Outflank (ask the operator for it if not already in your shell environment as e.g. `$COMMON_REPO_TOKEN_PAT` — do not fabricate or reuse an unrelated token):

```bash
gh secret set COMMON_REPO_TOKEN --repo TriYop/spellbound-MaxDPS
```

```bash
gh secret list --repo TriYop/spellbound-MaxDPS   # re-verify it now shows COMMON_REPO_TOKEN
```

Do this now, before Task 6's final CI-verification step, exactly to avoid the day-long red-CI gap Tank and Outflank both hit.

- [ ] **Step 3: Confirm the JUCE baseline still builds (post-Task-1 sanity check)**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
g++ -std=c++20 -O2 -I Source -I Tests Tests/test_transientshapercore.cpp Source/DSP/TransientShaperCore.cpp -o /tmp/test_tsc && /tmp/test_tsc
```

Expected: the JUCE plugin still builds (Task 1 didn't touch `CMakeLists.txt` permanently) and `test_transientshapercore` still passes. If this fails, stop — a dirty baseline makes every later failure ambiguous.

- [ ] **Step 4: Copy the DPF patches from Hex's `main` branch verbatim**

```bash
mkdir -p cmake/patches
cp /home/yvan/Projects/AudioPlugins/Hex/cmake/apply_patch.cmake cmake/apply_patch.cmake
cp /home/yvan/Projects/AudioPlugins/Hex/cmake/patches/dpf-clap-state-chunked-read.patch cmake/patches/dpf-clap-state-chunked-read.patch
cp /home/yvan/Projects/AudioPlugins/Hex/cmake/patches/dpf-clap-activate-latency.patch cmake/patches/dpf-clap-activate-latency.patch
```

Per the Global Constraints, do **not** copy Tank's third patch (`dpf-clap-latency-deactivate-report.patch`) — Bastos reports no latency, so the bug it fixes (illegal `clap_host_latency::changed()` calls outside `activate()`) has no runtime call site here.

- [ ] **Step 5: Move the JUCE-era source to `Source/_juce_reference/`**

```bash
git mv Source/PluginProcessor.h   Source/_juce_reference/PluginProcessor.h
git mv Source/PluginProcessor.cpp Source/_juce_reference/PluginProcessor.cpp
git mv Source/PluginEditor.h      Source/_juce_reference/PluginEditor.h
git mv Source/PluginEditor.cpp    Source/_juce_reference/PluginEditor.cpp
```

(`Source/_juce_reference/DSP/TransientShaper.h/.cpp` are already there from Task 1.) `Source/DSP/TransientShaperCore.h/.cpp` stay exactly where they are.

- [ ] **Step 6: Write `Source/DistrhoPluginInfo.h`**

```cpp
/*
 * Spellbound Bastos — DPF plugin metadata.
 *
 * Bastos is a stereo effect, not a synth: it does not want MIDI input, and
 * it processes two audio inputs into two audio outputs. It reports zero
 * latency -- TransientShaperCore (Source/DSP/TransientShaperCore.h) has no
 * delay lines, only per-sample IIR/envelope math.
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Spellbound"
#define DISTRHO_PLUGIN_NAME    "Bastos"
#define DISTRHO_PLUGIN_URI     "https://spellbound.audio/plugins/bastos"
#define DISTRHO_PLUGIN_CLAP_ID "com.spellbound.bastos"

#define DISTRHO_PLUGIN_BRAND_ID  Spbd
#define DISTRHO_PLUGIN_UNIQUE_ID Bsts

#define DISTRHO_PLUGIN_HAS_UI      1
#define DISTRHO_PLUGIN_IS_RT_SAFE  1
#define DISTRHO_PLUGIN_IS_SYNTH    0
#define DISTRHO_PLUGIN_NUM_INPUTS  2
#define DISTRHO_PLUGIN_NUM_OUTPUTS 2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 0
#define DISTRHO_PLUGIN_WANT_LATENCY    0

/*
 * Input/Output peak meters are never declared as a host parameter (or DPF
 * State) on any format -- clap-validator fails any host-visible, audio-
 * reactive output parameter regardless of hints (see Hex's CLAUDE.md's
 * param-set-events/param-set-no-cookies fix). Instead BastosUI reads
 * BastosPluginAdapter directly every idle tick via
 * DISTRHO_PLUGIN_WANT_DIRECT_ACCESS + UI::getPluginInstancePointer() -- same
 * idiom Hex/Tank use.
 */
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1
// Forced to 0 for the same reason documented in every prior migration's
// DistrhoPluginInfo.h: DPF defaults this to DISTRHO_PLUGIN_WANT_DIRECT_ACCESS's
// value when unset, which is wrong for a CMake build producing two separate
// LV2 modules (Bastos_dsp.so, Bastos_ui.so), not one combined object.
#define DISTRHO_PLUGIN_AND_UI_IN_SINGLE_OBJECT 0

#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Dynamics"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "audio-effect", "utility", "transient-shaper"

#define DISTRHO_UI_USE_NANOVG     1
#define DISTRHO_UI_USER_RESIZABLE 0
#define DISTRHO_UI_DEFAULT_WIDTH  660
#define DISTRHO_UI_DEFAULT_HEIGHT 460
#define DISTRHO_UI_FILE_BROWSER   1

/*
 * Host parameter indices, shared between BastosPluginAdapter and BastosUI.
 * Declared here (not in BastosPluginAdapter.h) so BastosUI.cpp can use them
 * without pulling in DistrhoPlugin.hpp — same placement Hex/Tank/Outflank use.
 *
 * NOTE: this is the REAL, shipped parameter set (verified by reading
 * Source/_juce_reference/PluginProcessor.cpp::createParameterLayout()
 * directly) -- NOT the fictional bass_enabled/drive/cutoff/blend/
 * transient_enabled/bp_* parameters the pre-migration CLAUDE.md described.
 * There is no bypass parameter of any kind.
 */
enum BastosParameters {
    kParameterAtkGain = 0,
    kParameterAtkSubCount,
    kParameterAtkSubLevel,
    kParameterAtkUpperCount,
    kParameterAtkUpperLevel,
    kParameterSusGain,
    kParameterSusSubCount,
    kParameterSusSubLevel,
    kParameterSusUpperCount,
    kParameterSusUpperLevel,
    kParameterSpeed,
    kParameterOutputGain,
    kParameterMix,
    kParameterCount   // 13
};

/* Ranges/defaults carried over verbatim from the JUCE-era
   Source/_juce_reference/PluginProcessor.cpp::createParameterLayout(). */
#define BASTOS_PARAM_ATK_GAIN_MIN      -12.0f
#define BASTOS_PARAM_ATK_GAIN_MAX      12.0f
#define BASTOS_PARAM_ATK_GAIN_DEFAULT  0.0f

#define BASTOS_PARAM_ATK_SUB_COUNT_MIN     1.0f
#define BASTOS_PARAM_ATK_SUB_COUNT_MAX     3.0f
#define BASTOS_PARAM_ATK_SUB_COUNT_DEFAULT 1.0f

#define BASTOS_PARAM_ATK_SUB_LEVEL_MIN     0.0f
#define BASTOS_PARAM_ATK_SUB_LEVEL_MAX     1.0f
#define BASTOS_PARAM_ATK_SUB_LEVEL_DEFAULT 0.0f

#define BASTOS_PARAM_ATK_UPPER_COUNT_MIN     1.0f
#define BASTOS_PARAM_ATK_UPPER_COUNT_MAX     5.0f
#define BASTOS_PARAM_ATK_UPPER_COUNT_DEFAULT 1.0f

#define BASTOS_PARAM_ATK_UPPER_LEVEL_MIN     0.0f
#define BASTOS_PARAM_ATK_UPPER_LEVEL_MAX     1.0f
#define BASTOS_PARAM_ATK_UPPER_LEVEL_DEFAULT 0.0f

#define BASTOS_PARAM_SUS_GAIN_MIN      -12.0f
#define BASTOS_PARAM_SUS_GAIN_MAX      12.0f
#define BASTOS_PARAM_SUS_GAIN_DEFAULT  0.0f

#define BASTOS_PARAM_SUS_SUB_COUNT_MIN     1.0f
#define BASTOS_PARAM_SUS_SUB_COUNT_MAX     3.0f
#define BASTOS_PARAM_SUS_SUB_COUNT_DEFAULT 1.0f

#define BASTOS_PARAM_SUS_SUB_LEVEL_MIN     0.0f
#define BASTOS_PARAM_SUS_SUB_LEVEL_MAX     1.0f
#define BASTOS_PARAM_SUS_SUB_LEVEL_DEFAULT 0.0f

#define BASTOS_PARAM_SUS_UPPER_COUNT_MIN     1.0f
#define BASTOS_PARAM_SUS_UPPER_COUNT_MAX     5.0f
#define BASTOS_PARAM_SUS_UPPER_COUNT_DEFAULT 1.0f

#define BASTOS_PARAM_SUS_UPPER_LEVEL_MIN     0.0f
#define BASTOS_PARAM_SUS_UPPER_LEVEL_MAX     1.0f
#define BASTOS_PARAM_SUS_UPPER_LEVEL_DEFAULT 0.0f

#define BASTOS_PARAM_SPEED_MIN     0.0f
#define BASTOS_PARAM_SPEED_MAX     1.0f
#define BASTOS_PARAM_SPEED_DEFAULT 0.0f

#define BASTOS_PARAM_OUTPUT_GAIN_MIN      -12.0f
#define BASTOS_PARAM_OUTPUT_GAIN_MAX      12.0f
#define BASTOS_PARAM_OUTPUT_GAIN_DEFAULT  0.0f

#define BASTOS_PARAM_MIX_MIN     0.0f
#define BASTOS_PARAM_MIX_MAX     1.0f
#define BASTOS_PARAM_MIX_DEFAULT 1.0f

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
```

- [ ] **Step 7: Write stub `Source/BastosPluginAdapter.h`/`.cpp` and `Source/BastosUI.h`/`.cpp`**

```cpp
// Source/BastosPluginAdapter.h
#pragma once

#include "DistrhoPlugin.hpp"

START_NAMESPACE_DISTRHO

class BastosPluginAdapter : public Plugin
{
public:
    BastosPluginAdapter() : Plugin(kParameterCount, 0, 0) {}

protected:
    const char* getLabel() const override { return "Bastos"; }
    const char* getDescription() const override { return "Bass enhancer and transient shaper"; }
    const char* getMaker() const override { return "Spellbound"; }
    const char* getLicense() const override { return "https://spellbound.audio/plugins/bastos#license"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    void initParameter(uint32_t, Parameter&) override {}
    float getParameterValue(uint32_t) const override { return 0.0f; }
    void setParameterValue(uint32_t, float) override {}

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        // Stub: pass through untouched. Task 3 wires TransientShaperCore.
        if (outputs[0] != inputs[0]) std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);
        if (outputs[1] != inputs[1]) std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BastosPluginAdapter)
};

END_NAMESPACE_DISTRHO
```

```cpp
// Source/BastosPluginAdapter.cpp
#include "BastosPluginAdapter.h"
#include <cstring>

START_NAMESPACE_DISTRHO
Plugin* createPlugin() { return new BastosPluginAdapter(); }
END_NAMESPACE_DISTRHO
```

```cpp
// Source/BastosUI.h
#pragma once

#include "DistrhoUI.hpp"

START_NAMESPACE_DISTRHO

class BastosUI : public UI
{
public:
    BastosUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {}

protected:
    void parameterChanged(uint32_t, float) override {}

    void onNanoDisplay() override
    {
        beginPath();
        rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
        fillColor(DGL_NAMESPACE::Color(0x14, 0x14, 0x20)); // kBg 0xff141420, see Task 4
        fill();
        closePath();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BastosUI)
};

END_NAMESPACE_DISTRHO
```

```cpp
// Source/BastosUI.cpp
#include "BastosUI.h"

START_NAMESPACE_DISTRHO
UI* createUI() { return new BastosUI(); }
END_NAMESPACE_DISTRHO
```

- [ ] **Step 8: Rewrite `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.22)
project(Bastos VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# DPF has no tagged releases -- pin the exact commit SHA Hex/Pugilist/Tank/
# Outflank all use.
set(AUDIOPLUGINS_DPF_GIT_TAG "4238e1c7f0351bbe488d79f0899c540543ac7583" CACHE STRING "Pinned DPF commit")

# CAVEAT: the patch step only runs on a fresh population of build/_deps/dpf-src.
# Delete it (or all of build/) when you need to be sure the patches took effect;
# cmake/apply_patch.cmake is idempotent but that's not a substitute for a clean re-fetch.
FetchContent_Declare(dpf
    GIT_REPOSITORY https://github.com/DISTRHO/DPF.git
    GIT_TAG        ${AUDIOPLUGINS_DPF_GIT_TAG}
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-state-chunked-read.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
            COMMAND ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-activate-latency.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
)
FetchContent_MakeAvailable(dpf)

set(BASTOS_DPF_TARGETS vst3 clap lv2)
if(APPLE)
    list(APPEND BASTOS_DPF_TARGETS au)
endif()

dpf_add_plugin(Bastos
    TARGETS ${BASTOS_DPF_TARGETS}
    UI_TYPE opengl
    USE_FILE_BROWSER TRUE
    FILES_DSP
        Source/BastosPluginAdapter.cpp
        Source/DSP/TransientShaperCore.cpp
    FILES_UI
        Source/BastosUI.cpp
)

target_include_directories(Bastos PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# DPF's dpf_add_plugin() unconditionally compiles a WebViewImpl.cpp that uses
# the GNU `typeof` extension and fails under strict ISO C++ -- same workaround
# every prior migration in this workspace needs (see Hex's CMakeLists.txt).
if(TARGET dgl-opengl)
    set_target_properties(dgl-opengl PROPERTIES CXX_EXTENSIONS ON)
endif()

# ── Shared HUI library ────────────────────────────────────────────────────
# ORDERING (load-bearing): must come after dpf_add_plugin(Bastos ...), which
# is what lazily creates DPF's dgl-opengl target that AudioPluginsCommon::
# hui_dgl is guarded on.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_Declare(AudioPluginsCommon
    GIT_REPOSITORY https://github.com/TriYop/spellbound-common.git
    GIT_TAG        v0.3.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(AudioPluginsCommon)

# Linked into `Bastos-ui` (not `Bastos`): dpf_add_plugin() splits the plugin
# into a common `Bastos` static lib plus `Bastos-dsp`/`Bastos-ui`; BastosUI.cpp
# is the only consumer of the widgets/presets and lives in `Bastos-ui`.
target_link_libraries(Bastos-ui PRIVATE AudioPluginsCommon::hui_dgl AudioPluginsCommon::presets)

# ── Unit tests (no JUCE/DPF dependency) ──────────────────────────────────
enable_testing()

add_executable(test_transientshapercore
    Tests/test_transientshapercore.cpp
    Source/DSP/TransientShaperCore.cpp
)
target_include_directories(test_transientshapercore PRIVATE Source/ Tests/)
target_compile_features(test_transientshapercore PRIVATE cxx_std_20)
add_test(NAME TransientShaperCore COMMAND test_transientshapercore)

# Task 5's FactoryPresets test target is appended here once it exists --
# deliberately not included yet, so this task's own build-verification step
# only references files that exist at this point in the plan.

# ── Install rules ─────────────────────────────────────────────────────────
# DPF writes all format outputs under a flat bin/ dir -- no Standalone
# target: Bastos is an effect, and this workspace only requires Standalone
# for instruments.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(_BIN "${CMAKE_BINARY_DIR}/bin")

install(DIRECTORY  "${_BIN}/Bastos.vst3"
        DESTINATION VST3
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

if(APPLE)
    install(DIRECTORY  "${_BIN}/Bastos.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            USE_SOURCE_PERMISSIONS)
else()
    install(FILES      "${_BIN}/Bastos.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

install(DIRECTORY  "${_BIN}/Bastos.lv2"
        DESTINATION LV2
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

install(PROGRAMS   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install.sh"
                   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/uninstall.sh"
        DESTINATION .
        COMPONENT   Runtime)

# ── CPack (TGZ) ─────────────────────────────────────────────────────────────
set(CPACK_GENERATOR                 TGZ)
set(CPACK_PACKAGE_NAME              Bastos)
set(CPACK_PACKAGE_VENDOR            Spellbound)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Bass enhancer and transient shaper")
set(CPACK_PACKAGE_VERSION           ${PROJECT_VERSION})
set(CPACK_SYSTEM_NAME               linux-x86_64)
set(CPACK_PACKAGE_FILE_NAME         "Bastos-${PROJECT_VERSION}-linux-x86_64")
set(CPACK_PACKAGING_INSTALL_PREFIX  "")
set(CPACK_STRIP_FILES               TRUE)
set(CPACK_PACKAGE_CHECKSUM          SHA256)
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_BINARY_DIR};${PROJECT_NAME};Runtime;/")
include(CPack)
```

- [ ] **Step 9: Update `scripts/install.sh`/`uninstall.sh` for the DPF layout (no Standalone, add LV2)**

Follow Outflank's Step 9 pattern exactly (`AudioPlugins/Outflank/scripts/install.sh`/`uninstall.sh` as merged reference): drop the `bin/Bastos` Standalone block, keep VST3/CLAP, add an LV2 block with `LV2_DIR` (`${HOME}/.lv2` / `/usr/lib/lv2`) alongside `VST3_DIR`/`CLAP_DIR`. Update the header comment to `# Install Bastos plugins (VST3/CLAP/LV2). Bastos is an effect -- no standalone app.`

- [ ] **Step 10: Write build-only CI at `.github/workflows/ci.yml`**

Copy Outflank's `.github/workflows/ci.yml` build-only structure verbatim (Linux-gating, Windows/macOS `continue-on-error`, `xvfb` in the Linux apt install list), substituting `Bastos`/`spellbound-MaxDPS` for `Outflank`/`spellbound-outflank`, and include the "Configure Common repo access" step from the start (Outflank/Tank added it in a later task; add it here immediately since Step 2 already set the secret):

```yaml
name: CI

on:
  push:
    branches: [ "main", "master" ]
    tags: [ "v*" ]
  pull_request:
    branches: [ "main", "master" ]

jobs:
  build-and-test:
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        build_type: [Release]

    runs-on: ${{ matrix.os }}
    continue-on-error: ${{ matrix.os != 'ubuntu-latest' }}

    steps:
      - uses: actions/checkout@v4

      - name: Install Linux build dependencies
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            libasound2-dev libjack-jackd2-dev \
            libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
            libxinerama-dev libxrandr-dev libxrender-dev \
            libfreetype-dev libfontconfig1-dev \
            libglu1-mesa-dev libwebkit2gtk-4.1-dev \
            xvfb

      - name: Configure Common repo access
        # github.com/TriYop/spellbound-common (FetchContent'd below) is
        # private -- rewrite its clone URL to embed the COMMON_REPO_TOKEN
        # secret set in Task 2 Step 2. Verify with `gh secret list --repo
        # TriYop/spellbound-MaxDPS` before trusting a green run.
        run: git config --global url."https://x-access-token:${{ secrets.COMMON_REPO_TOKEN }}@github.com/TriYop/spellbound-common".insteadOf "https://github.com/TriYop/spellbound-common"

      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}

      - name: Build
        run: cmake --build build --config ${{ matrix.build_type }} --parallel

      - name: Test
        working-directory: build
        run: ctest --build-config ${{ matrix.build_type }} --output-on-failure
```

Do **not** add pluginval/clap-validator/lv2lint yet — Task 6 adds those once there's real DSP/UI to validate.

- [ ] **Step 11: Configure, build, and test to verify the skeleton compiles**

```bash
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected: DPF + Common fetch successfully, `build/bin/Bastos.vst3`/`.clap`/`.lv2` all produced, `TransientShaperCore` (1/1) passes.

- [ ] **Step 12: Commit**

```bash
git add -A
git commit -m "$(cat <<'EOF'
Stage 0: DPF skeleton for Bastos (stub adapter/UI, build-only CI)

Drops JUCE + clap-juce-extensions in favor of DPF (pinned commit + 2
upstream patches) and AudioPlugins/Common v0.3.0, following Hex/Pugilist/
Tank/Outflank's proven pattern. Rebrands from the legacy YvanJanet/Yjnt
identity to Spellbound/Spbd, matching every other migrated plugin.
PluginProcessor/PluginEditor preserved unchanged under Source/
_juce_reference/ as the porting reference (alongside Task 1's
_juce_reference/DSP/TransientShaper.h/.cpp). BastosPluginAdapter/BastosUI
are Stage 0 stubs (passthrough audio, blank UI) -- Task 3 wires the real
TransientShaperCore chain.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Wire `TransientShaperCore` and all 13 parameters into `BastosPluginAdapter`

**Files:**
- Modify: `Source/BastosPluginAdapter.h` (replace the Task 2 stub)
- Modify: `Source/BastosPluginAdapter.cpp` (replace the Task 2 stub)

**Interfaces:**
- Consumes: `TransientShaperCore` (Task 1), `BastosParameters`/`BASTOS_PARAM_*` (Task 2's `DistrhoPluginInfo.h`), `audioplugins::common::io::MeterTransport` (`Common`, linked via `AudioPluginsCommon::io` — add this link in this task, see Step 1).
- Produces: `BastosPluginAdapter::getInputLevel()/getOutputLevel() const noexcept -> float`, consumed by `BastosUI` in Task 4.

- [ ] **Step 1: Add `AudioPluginsCommon::io` to `CMakeLists.txt`**

`MeterTransport` (input/output peak level) needs a real link, not just an include path — insert immediately after the `target_link_libraries(Bastos-ui ...)` line from Task 2 Step 8:

```cmake
target_link_libraries(Bastos PUBLIC AudioPluginsCommon::io)
```

- [ ] **Step 2: Write `Source/BastosPluginAdapter.h`**

```cpp
#pragma once

#include "DistrhoPlugin.hpp"
#include "DSP/TransientShaperCore.h"
#include "audioplugins/common/io/MeterTransport.h"

#include <vector>

START_NAMESPACE_DISTRHO

/**
   DPF Plugin adapter for Spellbound Bastos.

   Thin shim over TransientShaperCore (Source/DSP/TransientShaperCore.h,
   framework-free): reads all 13 host parameters, calls
   TransientShaperCore::process() once per block, then replicates the
   block-level logic the JUCE-era PluginProcessor::processBlock() did
   directly (dry/wet mix, output gain, input/output peak metering) -- none
   of that block-level logic was JUCE-dependent beyond
   juce::Decibels::gainToDecibels/decibelsToGain and juce::AudioBuffer
   copies, both trivially reimplemented here without any framework.

   Peak meters are reported via MeterTransport, not a DPF parameter (see
   DistrhoPluginInfo.h's DISTRHO_PLUGIN_WANT_DIRECT_ACCESS comment) --
   same idiom as Hex's IN/OUT meters.
 */
class BastosPluginAdapter : public Plugin
{
public:
    BastosPluginAdapter();

protected:
    // -- Information -----------------------------------------------------
    const char* getLabel() const override;
    const char* getDescription() const override;
    const char* getMaker() const override;
    const char* getLicense() const override;
    uint32_t getVersion() const override;

    // -- Init -------------------------------------------------------------
    void initParameter(uint32_t index, Parameter& parameter) override;

    // -- Internal data ------------------------------------------------------
    float getParameterValue(uint32_t index) const override;
    void setParameterValue(uint32_t index, float value) override;

public:
    // -- Direct UI access (DISTRHO_PLUGIN_WANT_DIRECT_ACCESS) ---------------
    // Read by BastosUI via getPluginInstancePointer() + uiIdle().
    float getInputLevel() const noexcept { return inputLevelMeter_.read(); }
    float getOutputLevel() const noexcept { return outputLevelMeter_.read(); }

protected:
    // -- Process ------------------------------------------------------------
    void activate() override;
    void deactivate() override;
    void bufferSizeChanged(uint32_t newBufferSize) override;
    void run(const float** inputs, float** outputs, uint32_t frames) override;

private:
    float atkGainDb_       = BASTOS_PARAM_ATK_GAIN_DEFAULT;
    float atkSubCount_     = BASTOS_PARAM_ATK_SUB_COUNT_DEFAULT;
    float atkSubLevel_     = BASTOS_PARAM_ATK_SUB_LEVEL_DEFAULT;
    float atkUpperCount_   = BASTOS_PARAM_ATK_UPPER_COUNT_DEFAULT;
    float atkUpperLevel_   = BASTOS_PARAM_ATK_UPPER_LEVEL_DEFAULT;
    float susGainDb_       = BASTOS_PARAM_SUS_GAIN_DEFAULT;
    float susSubCount_     = BASTOS_PARAM_SUS_SUB_COUNT_DEFAULT;
    float susSubLevel_     = BASTOS_PARAM_SUS_SUB_LEVEL_DEFAULT;
    float susUpperCount_   = BASTOS_PARAM_SUS_UPPER_COUNT_DEFAULT;
    float susUpperLevel_   = BASTOS_PARAM_SUS_UPPER_LEVEL_DEFAULT;
    float speed_           = BASTOS_PARAM_SPEED_DEFAULT;
    float outputGainDb_    = BASTOS_PARAM_OUTPUT_GAIN_DEFAULT;
    float mix_             = BASTOS_PARAM_MIX_DEFAULT;

    TransientShaperCore shaper_;

    // Scratch dry-signal buffers for the mix knob, resized (never allocated
    // in run() itself) in activate()/bufferSizeChanged() to stay RT-safe.
    std::vector<float> dryLeft_;
    std::vector<float> dryRight_;

    audioplugins::common::io::MeterTransport inputLevelMeter_ { "input_level" };
    audioplugins::common::io::MeterTransport outputLevelMeter_ { "output_level" };

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BastosPluginAdapter)
};

END_NAMESPACE_DISTRHO
```

- [ ] **Step 3: Write `Source/BastosPluginAdapter.cpp`**

```cpp
#include "BastosPluginAdapter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

START_NAMESPACE_DISTRHO

namespace {
float dbToGain(float db) noexcept { return std::pow(10.f, db / 20.f); }
float gainToDb(float gain) noexcept { return gain > 1e-7f ? 20.f * std::log10(gain) : -100.f; }
}

BastosPluginAdapter::BastosPluginAdapter()
    : Plugin(kParameterCount, 0, 0) // 13 parameters, 0 programs, 0 states -- meters are direct-access only
{
}

const char* BastosPluginAdapter::getLabel() const { return "Bastos"; }
const char* BastosPluginAdapter::getDescription() const { return "Bass enhancer and transient shaper"; }
const char* BastosPluginAdapter::getMaker() const { return "Spellbound"; }

const char* BastosPluginAdapter::getLicense() const
{
    // Must be a URI -- lv2lint's Plugin License test fails a plain word
    // like "Proprietary" (confirmed against Hex/Tank/Outflank).
    return "https://spellbound.audio/plugins/bastos#license";
}

uint32_t BastosPluginAdapter::getVersion() const { return d_version(0, 1, 0); }

void BastosPluginAdapter::initParameter(const uint32_t index, Parameter& parameter)
{
    switch (index)
    {
    case kParameterAtkGain:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Attack Gain"; parameter.symbol = "atk_gain"; parameter.unit = "dB";
        parameter.ranges.def = BASTOS_PARAM_ATK_GAIN_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_ATK_GAIN_MIN;
        parameter.ranges.max = BASTOS_PARAM_ATK_GAIN_MAX;
        break;
    case kParameterAtkSubCount:
        parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
        parameter.name = "Attack Sub Count"; parameter.symbol = "atk_sub_count";
        parameter.ranges.def = BASTOS_PARAM_ATK_SUB_COUNT_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_ATK_SUB_COUNT_MIN;
        parameter.ranges.max = BASTOS_PARAM_ATK_SUB_COUNT_MAX;
        break;
    case kParameterAtkSubLevel:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Attack Sub Level"; parameter.symbol = "atk_sub_level";
        parameter.ranges.def = BASTOS_PARAM_ATK_SUB_LEVEL_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_ATK_SUB_LEVEL_MIN;
        parameter.ranges.max = BASTOS_PARAM_ATK_SUB_LEVEL_MAX;
        break;
    case kParameterAtkUpperCount:
        parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
        parameter.name = "Attack Upper Count"; parameter.symbol = "atk_upper_count";
        parameter.ranges.def = BASTOS_PARAM_ATK_UPPER_COUNT_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_ATK_UPPER_COUNT_MIN;
        parameter.ranges.max = BASTOS_PARAM_ATK_UPPER_COUNT_MAX;
        break;
    case kParameterAtkUpperLevel:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Attack Upper Level"; parameter.symbol = "atk_upper_level";
        parameter.ranges.def = BASTOS_PARAM_ATK_UPPER_LEVEL_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_ATK_UPPER_LEVEL_MIN;
        parameter.ranges.max = BASTOS_PARAM_ATK_UPPER_LEVEL_MAX;
        break;
    case kParameterSusGain:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Sustain Gain"; parameter.symbol = "sus_gain"; parameter.unit = "dB";
        parameter.ranges.def = BASTOS_PARAM_SUS_GAIN_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SUS_GAIN_MIN;
        parameter.ranges.max = BASTOS_PARAM_SUS_GAIN_MAX;
        break;
    case kParameterSusSubCount:
        parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
        parameter.name = "Sustain Sub Count"; parameter.symbol = "sus_sub_count";
        parameter.ranges.def = BASTOS_PARAM_SUS_SUB_COUNT_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SUS_SUB_COUNT_MIN;
        parameter.ranges.max = BASTOS_PARAM_SUS_SUB_COUNT_MAX;
        break;
    case kParameterSusSubLevel:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Sustain Sub Level"; parameter.symbol = "sus_sub_level";
        parameter.ranges.def = BASTOS_PARAM_SUS_SUB_LEVEL_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SUS_SUB_LEVEL_MIN;
        parameter.ranges.max = BASTOS_PARAM_SUS_SUB_LEVEL_MAX;
        break;
    case kParameterSusUpperCount:
        parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
        parameter.name = "Sustain Upper Count"; parameter.symbol = "sus_upper_count";
        parameter.ranges.def = BASTOS_PARAM_SUS_UPPER_COUNT_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SUS_UPPER_COUNT_MIN;
        parameter.ranges.max = BASTOS_PARAM_SUS_UPPER_COUNT_MAX;
        break;
    case kParameterSusUpperLevel:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Sustain Upper Level"; parameter.symbol = "sus_upper_level";
        parameter.ranges.def = BASTOS_PARAM_SUS_UPPER_LEVEL_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SUS_UPPER_LEVEL_MIN;
        parameter.ranges.max = BASTOS_PARAM_SUS_UPPER_LEVEL_MAX;
        break;
    case kParameterSpeed:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Speed"; parameter.symbol = "speed";
        parameter.ranges.def = BASTOS_PARAM_SPEED_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SPEED_MIN;
        parameter.ranges.max = BASTOS_PARAM_SPEED_MAX;
        break;
    case kParameterOutputGain:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Output Gain"; parameter.symbol = "output_gain"; parameter.unit = "dB";
        parameter.ranges.def = BASTOS_PARAM_OUTPUT_GAIN_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_OUTPUT_GAIN_MIN;
        parameter.ranges.max = BASTOS_PARAM_OUTPUT_GAIN_MAX;
        break;
    case kParameterMix:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Mix"; parameter.symbol = "mix";
        parameter.ranges.def = BASTOS_PARAM_MIX_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_MIX_MIN;
        parameter.ranges.max = BASTOS_PARAM_MIX_MAX;
        break;
    default:
        break;
    }
}

float BastosPluginAdapter::getParameterValue(const uint32_t index) const
{
    switch (index)
    {
    case kParameterAtkGain:       return atkGainDb_;
    case kParameterAtkSubCount:   return atkSubCount_;
    case kParameterAtkSubLevel:   return atkSubLevel_;
    case kParameterAtkUpperCount: return atkUpperCount_;
    case kParameterAtkUpperLevel: return atkUpperLevel_;
    case kParameterSusGain:       return susGainDb_;
    case kParameterSusSubCount:   return susSubCount_;
    case kParameterSusSubLevel:   return susSubLevel_;
    case kParameterSusUpperCount: return susUpperCount_;
    case kParameterSusUpperLevel: return susUpperLevel_;
    case kParameterSpeed:         return speed_;
    case kParameterOutputGain:    return outputGainDb_;
    case kParameterMix:           return mix_;
    default:                      return 0.0f;
    }
}

void BastosPluginAdapter::setParameterValue(const uint32_t index, const float value)
{
    switch (index)
    {
    case kParameterAtkGain:       atkGainDb_ = value; break;
    case kParameterAtkSubCount:   atkSubCount_ = value; break;
    case kParameterAtkSubLevel:   atkSubLevel_ = value; break;
    case kParameterAtkUpperCount: atkUpperCount_ = value; break;
    case kParameterAtkUpperLevel: atkUpperLevel_ = value; break;
    case kParameterSusGain:       susGainDb_ = value; break;
    case kParameterSusSubCount:   susSubCount_ = value; break;
    case kParameterSusSubLevel:   susSubLevel_ = value; break;
    case kParameterSusUpperCount: susUpperCount_ = value; break;
    case kParameterSusUpperLevel: susUpperLevel_ = value; break;
    case kParameterSpeed:         speed_ = value; break;
    case kParameterOutputGain:    outputGainDb_ = value; break;
    case kParameterMix:           mix_ = value; break;
    default: break;
    }
}

void BastosPluginAdapter::activate()
{
    shaper_.prepare(getSampleRate());
    dryLeft_.assign(getBufferSize(), 0.f);
    dryRight_.assign(getBufferSize(), 0.f);
}

void BastosPluginAdapter::deactivate()
{
    shaper_.reset();
}

void BastosPluginAdapter::bufferSizeChanged(const uint32_t newBufferSize)
{
    dryLeft_.assign(newBufferSize, 0.f);
    dryRight_.assign(newBufferSize, 0.f);
}

void BastosPluginAdapter::run(const float** inputs, float** outputs, const uint32_t frames)
{
    // Input peak (matches the JUCE-era processBlock()'s per-block peak scan).
    float inPeak = 0.f;
    for (uint32_t i = 0; i < frames; ++i)
    {
        inPeak = std::max(inPeak, std::abs(inputs[0][i]));
        inPeak = std::max(inPeak, std::abs(inputs[1][i]));
    }
    inputLevelMeter_.write(gainToDb(inPeak));

    if (outputs[0] != inputs[0]) std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);
    if (outputs[1] != inputs[1]) std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);

    const bool wantsDryBlend = mix_ < 0.999f;
    if (wantsDryBlend)
    {
        std::memcpy(dryLeft_.data(),  outputs[0], sizeof(float) * frames);
        std::memcpy(dryRight_.data(), outputs[1], sizeof(float) * frames);
    }

    float* channels[2] = { outputs[0], outputs[1] };
    shaper_.process(channels, 2, static_cast<int>(frames),
                     atkGainDb_, static_cast<int>(atkSubCount_ + 0.5f), atkSubLevel_,
                     static_cast<int>(atkUpperCount_ + 0.5f), atkUpperLevel_,
                     susGainDb_, static_cast<int>(susSubCount_ + 0.5f), susSubLevel_,
                     static_cast<int>(susUpperCount_ + 0.5f), susUpperLevel_,
                     speed_, true);

    const float outGainLin = dbToGain(outputGainDb_);
    for (uint32_t i = 0; i < frames; ++i)
    {
        outputs[0][i] *= outGainLin;
        outputs[1][i] *= outGainLin;
    }

    if (wantsDryBlend)
    {
        const float dryAmount = 1.f - mix_;
        for (uint32_t i = 0; i < frames; ++i)
        {
            outputs[0][i] += dryAmount * dryLeft_[i];
            outputs[1][i] += dryAmount * dryRight_[i];
        }
    }

    float outPeak = 0.f;
    for (uint32_t i = 0; i < frames; ++i)
    {
        outPeak = std::max(outPeak, std::abs(outputs[0][i]));
        outPeak = std::max(outPeak, std::abs(outputs[1][i]));
    }
    outputLevelMeter_.write(gainToDb(outPeak));
}

Plugin* createPlugin() { return new BastosPluginAdapter(); }

END_NAMESPACE_DISTRHO
```

- [ ] **Step 4: Build and run the full test suite**

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected: `TransientShaperCore` still passes (unchanged), and the plugin now processes real audio through `Bastos.vst3`/`.clap`/`.lv2`.

- [ ] **Step 5: Commit**

```bash
git add Source/BastosPluginAdapter.h Source/BastosPluginAdapter.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
Wire TransientShaperCore and all 13 parameters into BastosPluginAdapter

Declares atk_gain/atk_sub_count/atk_sub_level/atk_upper_count/
atk_upper_level/sus_gain/sus_sub_count/sus_sub_level/sus_upper_count/
sus_upper_level/speed/output_gain/mix through DPF's Parameter API,
matching the JUCE-era createParameterLayout() ranges/defaults exactly.
run() replicates the JUCE processBlock()'s block-level dry/wet mix,
output gain, and input/output peak metering (all trivially framework-
free -- only TransientShaper itself needed extraction, per Task 1).

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Port the UI — 13 `RotaryKnob`s across Attack/Sustain/Global groups, 2 `VuMeter`s

**Widget audit (per this migration's scope check):** the current JUCE `PluginEditor` uses only rotary sliders (`juce::Slider::RotaryVerticalDrag`, both float and "int" variants — `atkSubCountKnob_` etc. are plain `Slider`s with `setNumDecimalPlacesToDisplay(0)`, not a distinct widget type) plus two custom-drawn level meters (`drawMeter()`) and section-background panels (`drawSectionBg()`, plain rounded rectangles + a text label, not a reusable widget). There is **no** bypass toggle, no bandpass controls, and no combo box anywhere in `Source/PluginEditor.h/.cpp` (confirmed by reading both files) — this matches Task 3's parameter audit and confirms `AudioPlugins/Common`'s existing widget set is sufficient: **`RotaryKnob`** (13x, `common/hui/dgl/RotaryKnob.h`) and **`VuMeter`** (2x, `Vertical` orientation — the default, same as Hex's IN/OUT meters, `common/hui/dgl/VuMeter.h`). Both already exist in Common v0.3.0. No new Common widget is needed and none is added — the section-background panels are simple enough to redraw as plain NanoVG rounded rects directly in `BastosUI::onNanoDisplay()`, same as every other migrated plugin's panel background.

**Files:**
- Modify: `Source/BastosUI.h`, `Source/BastosUI.cpp` (real 13-knob + 2-meter UI)

**Interfaces:**
- Consumes: `audioplugins::common::hui::dgl::RotaryKnob`/`RotaryKnobPalette`, `audioplugins::common::hui::dgl::VuMeter`/`VuMeterPalette` (both `Common`), `BastosParameters`/`BASTOS_PARAM_*` (Task 2), `BastosPluginAdapter::getInputLevel()/getOutputLevel()` (Task 3).
- Produces: `class BastosUI` now has 13 live knobs + 2 meters — consumed unchanged by Task 5 (extended with a presets bar, not replaced).

- [ ] **Step 1: Write the real `Source/BastosUI.h`**

```cpp
#pragma once

#include "DistrhoUI.hpp"
#include "BastosPluginAdapter.h"

#include "audioplugins/common/hui/dgl/RotaryKnob.h"
#include "audioplugins/common/hui/dgl/VuMeter.h"

#include <array>
#include <memory>

START_NAMESPACE_DISTRHO

/**
   DPF UI adapter for Spellbound Bastos.

   13 RotaryKnobs across three groups (Attack/Sustain/Global) plus 2
   VuMeters (IN/OUT), laid out to match the JUCE-era 660x460 editor's
   three-column layout (see Source/_juce_reference/PluginEditor.cpp).
   IN/OUT levels are polled every uiIdle() tick via
   DISTRHO_PLUGIN_WANT_DIRECT_ACCESS + getPluginInstancePointer(), same
   idiom as Hex's meters -- not a DPF parameter.
 */
class BastosUI : public UI
{
public:
    BastosUI();

protected:
    void parameterChanged(uint32_t index, float value) override;
    void onNanoDisplay() override;
    void uiIdle() override;

private:
    static constexpr int kNumKnobs = 13;
    std::array<std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob>, kNumKnobs> knobs_;
    std::unique_ptr<audioplugins::common::hui::dgl::VuMeter> inMeter_;
    std::unique_ptr<audioplugins::common::hui::dgl::VuMeter> outMeter_;

    void setKnobValue(uint32_t index, float value);

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BastosUI)
};

END_NAMESPACE_DISTRHO
```

- [ ] **Step 2: Write the real `Source/BastosUI.cpp`**

```cpp
#include "BastosUI.h"

START_NAMESPACE_DISTRHO

namespace {

namespace hui = audioplugins::common::hui;

DGL_NAMESPACE::Color toDglColor(const hui::Colour& c) noexcept
{
    return DGL_NAMESPACE::Color(static_cast<int>(c.r), static_cast<int>(c.g),
                                 static_cast<int>(c.b), static_cast<float>(c.a) / 255.f);
}

// Palette ported from Source/_juce_reference/PluginEditor.cpp's kAtkAccent/
// kSusAccent/kGlobAccent (blue/teal/gold), reused as knob accent colors.
constexpr hui::Colour kBg        { 0x14, 0x14, 0x20, 0xff };
constexpr hui::Colour kSectionBg { 0x1e, 0x1e, 0x30, 0xff };
constexpr hui::Colour kAtkAccent { 0x44, 0xaa, 0xee, 0xff };
constexpr hui::Colour kSusAccent { 0x44, 0xdd, 0xaa, 0xff };
constexpr hui::Colour kGlobAccent{ 0xaa, 0xaa, 0x66, 0xff };

hui::dgl::RotaryKnobPalette paletteFor(const hui::Colour& accent)
{
    hui::dgl::RotaryKnobPalette p;
    p.valueArc = accent;
    p.valueArcGlow = accent;
    p.knobRim = accent;
    return p;
}

// Layout ported from the JUCE-era resized(): kW=660, kH=460, kKnobSize=72,
// three ATTACK/SUSTAIN/GLOBAL columns plus a 2-meter strip on the right.
constexpr int kW = 660, kH = 460;
constexpr int kHeaderH = 36, kMargin = 10, kGap = 8;
constexpr int kKnobSize = 72, kLabelH = 16, kMeterW = 14;
constexpr int kAtkW = 240, kSusW = 240;

} // namespace

BastosUI::BastosUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
{
    struct KnobSpec { float min, max, def; hui::Colour accent; int col, row; };
    static constexpr KnobSpec specs[kNumKnobs] = {
        { BASTOS_PARAM_ATK_GAIN_MIN, BASTOS_PARAM_ATK_GAIN_MAX, BASTOS_PARAM_ATK_GAIN_DEFAULT, kAtkAccent, 0, 0 },
        { BASTOS_PARAM_ATK_SUB_COUNT_MIN, BASTOS_PARAM_ATK_SUB_COUNT_MAX, BASTOS_PARAM_ATK_SUB_COUNT_DEFAULT, kAtkAccent, 0, 1 },
        { BASTOS_PARAM_ATK_SUB_LEVEL_MIN, BASTOS_PARAM_ATK_SUB_LEVEL_MAX, BASTOS_PARAM_ATK_SUB_LEVEL_DEFAULT, kAtkAccent, 0, 1 },
        { BASTOS_PARAM_ATK_UPPER_COUNT_MIN, BASTOS_PARAM_ATK_UPPER_COUNT_MAX, BASTOS_PARAM_ATK_UPPER_COUNT_DEFAULT, kAtkAccent, 0, 2 },
        { BASTOS_PARAM_ATK_UPPER_LEVEL_MIN, BASTOS_PARAM_ATK_UPPER_LEVEL_MAX, BASTOS_PARAM_ATK_UPPER_LEVEL_DEFAULT, kAtkAccent, 0, 2 },
        { BASTOS_PARAM_SUS_GAIN_MIN, BASTOS_PARAM_SUS_GAIN_MAX, BASTOS_PARAM_SUS_GAIN_DEFAULT, kSusAccent, 1, 0 },
        { BASTOS_PARAM_SUS_SUB_COUNT_MIN, BASTOS_PARAM_SUS_SUB_COUNT_MAX, BASTOS_PARAM_SUS_SUB_COUNT_DEFAULT, kSusAccent, 1, 1 },
        { BASTOS_PARAM_SUS_SUB_LEVEL_MIN, BASTOS_PARAM_SUS_SUB_LEVEL_MAX, BASTOS_PARAM_SUS_SUB_LEVEL_DEFAULT, kSusAccent, 1, 1 },
        { BASTOS_PARAM_SUS_UPPER_COUNT_MIN, BASTOS_PARAM_SUS_UPPER_COUNT_MAX, BASTOS_PARAM_SUS_UPPER_COUNT_DEFAULT, kSusAccent, 1, 2 },
        { BASTOS_PARAM_SUS_UPPER_LEVEL_MIN, BASTOS_PARAM_SUS_UPPER_LEVEL_MAX, BASTOS_PARAM_SUS_UPPER_LEVEL_DEFAULT, kSusAccent, 1, 2 },
        { BASTOS_PARAM_SPEED_MIN, BASTOS_PARAM_SPEED_MAX, BASTOS_PARAM_SPEED_DEFAULT, kGlobAccent, 2, 0 },
        { BASTOS_PARAM_OUTPUT_GAIN_MIN, BASTOS_PARAM_OUTPUT_GAIN_MAX, BASTOS_PARAM_OUTPUT_GAIN_DEFAULT, kGlobAccent, 2, 1 },
        { BASTOS_PARAM_MIX_MIN, BASTOS_PARAM_MIX_MAX, BASTOS_PARAM_MIX_DEFAULT, kGlobAccent, 2, 2 },
    };

    const int colX[3] = { kMargin, kMargin + kAtkW + kGap, kMargin + kAtkW + kGap + kSusW + kGap };
    const int bodyY = kHeaderH + kMargin;
    const int rowH  = kKnobSize + kLabelH + kGap;
    // Two knobs per row for the sub/upper pairs (rows 1/2); one per row 0/global.
    int slotOffsetInRow[kNumKnobs] = {0};
    {
        int rowSlot[3][3] = {{0,0,0},{0,0,0},{0,0,0}};
        for (int i = 0; i < kNumKnobs; ++i)
        {
            slotOffsetInRow[i] = rowSlot[specs[i].col][specs[i].row]++;
        }
    }

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto knob = std::make_unique<audioplugins::common::hui::dgl::RotaryKnob>(this);
        knob->setRange(specs[i].min, specs[i].max);
        knob->setValue(specs[i].def);
        knob->setPalette(paletteFor(specs[i].accent));

        const int x = colX[specs[i].col] + 4 + slotOffsetInRow[i] * (kKnobSize + 4);
        const int y = bodyY + 28 + specs[i].row * rowH;
        knob->setAbsolutePos(x, y);
        knob->setSize(kKnobSize, kKnobSize);

        const auto paramIndex = static_cast<uint32_t>(i);
        knob->setCallback([this, paramIndex](audioplugins::common::hui::dgl::RotaryKnob*, float value) {
            setParameterValue(paramIndex, value);
        });

        knobs_[static_cast<size_t>(i)] = std::move(knob);
    }

    inMeter_  = std::make_unique<audioplugins::common::hui::dgl::VuMeter>(this);
    outMeter_ = std::make_unique<audioplugins::common::hui::dgl::VuMeter>(this);
    const int meterX = kW - kMargin - 2 * kMeterW - kGap;
    const int meterY = bodyY + 20;
    const int meterH = kH - meterY - kMargin - 10;
    inMeter_->setAbsolutePos(meterX, meterY);
    inMeter_->setSize(kMeterW, static_cast<uint>(meterH));
    outMeter_->setAbsolutePos(meterX + kMeterW + kGap, meterY);
    outMeter_->setSize(kMeterW, static_cast<uint>(meterH));
}

void BastosUI::parameterChanged(const uint32_t index, const float value)
{
    setKnobValue(index, value);
}

void BastosUI::setKnobValue(const uint32_t index, const float value)
{
    if (index < static_cast<uint32_t>(kNumKnobs))
        knobs_[index]->setValue(value);
}

void BastosUI::uiIdle()
{
    // Direct-access read of the DSP instance's atomically-published peak
    // meters -- see DistrhoPluginInfo.h's DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
    // comment for why this isn't a DPF parameter.
    if (auto* plugin = getPluginInstancePointer<BastosPluginAdapter>())
    {
        inMeter_->pushLevel(plugin->getInputLevel());
        outMeter_->pushLevel(plugin->getOutputLevel());
    }
}

void BastosUI::onNanoDisplay()
{
    beginPath();
    rect(0, 0, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillColor(toDglColor(kBg));
    fill();
    closePath();

    // Section backgrounds -- plain rounded rects, ported from the JUCE-era
    // drawSectionBg() (no reusable "panel" widget exists in Common, and one
    // rounded rect + a label doesn't warrant adding one).
    const int bodyY = kHeaderH + kMargin;
    const int bodyH = kH - bodyY - kMargin;
    const int atkX = kMargin, susX = atkX + kAtkW + kGap, globX = susX + kSusW + kGap;
    const int globW = kW - globX - kMargin - 2 * kMeterW - kGap * 2;

    auto drawPanel = [this](int x, int y, int w, int h) {
        beginPath();
        roundedRect(static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(w), static_cast<float>(h), 6.f);
        fillColor(toDglColor(kSectionBg));
        fill();
        closePath();
    };
    drawPanel(atkX, bodyY, kAtkW, bodyH);
    drawPanel(susX, bodyY, kSusW, bodyH);
    drawPanel(globX, bodyY, globW, bodyH);
}

UI* createUI() { return new BastosUI(); }

END_NAMESPACE_DISTRHO
```

Note: `RotaryKnob::setCallback`/`setValue`/`setAbsolutePos`/`setSize` and `VuMeter::pushLevel`/`setAbsolutePos`/`setSize` are the same DGL `SubWidget` conventions used by Hex's `HexUI.cpp` — confirm the exact method names against `Common/include/audioplugins/common/hui/dgl/RotaryKnob.h` and `VuMeter.h` (and Hex's `HexUI.cpp` for real usage) while implementing this step, since a header-skim during planning is not a substitute for compiling against the real API.

- [ ] **Step 3: Build and manually smoke-test**

```bash
cmake --build build --parallel
```

Fix any compile errors against the real `RotaryKnob`/`VuMeter` API signatures (see the note in Step 2) before proceeding — this is exactly the kind of drift a plan written ahead of execution can get slightly wrong on method names.

- [ ] **Step 4: Commit**

```bash
git add Source/BastosUI.h Source/BastosUI.cpp
git commit -m "$(cat <<'EOF'
Port the Bastos UI to Common's RotaryKnob/VuMeter widgets

13 knobs (Attack/Sustain/Global groups, blue/teal/gold accents ported
from the JUCE-era PluginEditor's kAtkAccent/kSusAccent/kGlobAccent) plus
2 VuMeters (IN/OUT, Vertical orientation) polled via
DISTRHO_PLUGIN_WANT_DIRECT_ACCESS. No new Common widget needed -- the
JUCE editor never had a bypass toggle, bandpass control, or combo box
(confirmed by reading PluginEditor.h/.cpp), so RotaryKnob + VuMeter
cover the full widget surface.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Add a factory-presets panel (new feature — Bastos never had one)

**Presets audit:** the current JUCE `PluginProcessor` has no preset system at all — `getNumPrograms()` returns `1`, `getCurrentProgram()`/`setCurrentProgram()`/`getProgramName()`/`changeProgramName()` are all no-ops, and there is no `PresetManager` class anywhere in `Source/` (confirmed by directory listing and by reading `PluginProcessor.h/.cpp` in full). Per the Tank precedent (which added a presets panel it never had), this migration adds one for Bastos too: a `PresetSelector` dropdown + SAVE/DELETE `Button`s backed by `AudioPluginsCommon::presets::PresetBrowser`, with a handful of hand-authored factory presets targeting Bastos's stated use case (kick drum / low-frequency transient shaping, per `CLAUDE.md`'s project overview, which — unlike its stale parameter table — accurately describes the plugin's *purpose*).

**Files:**
- Create: `Source/FactoryPresets.h`
- Modify: `Source/BastosUI.h`, `Source/BastosUI.cpp` (add the presets bar, canvas grows to fit it)
- Modify: `Source/DistrhoPluginInfo.h` (`DISTRHO_UI_DEFAULT_HEIGHT` grows to fit the preset bar, matching Hex's precedent)
- Modify: `.github/workflows/ci.yml` is unaffected (Common repo access already configured in Task 2)

**Interfaces:**
- Consumes: `audioplugins::common::presets::Preset`/`PresetBrowser` and `audioplugins::common::hui::dgl::PresetSelector`/`Button` (all `Common`, already linked via `AudioPluginsCommon::presets`/`hui_dgl` from Task 2 Step 8).
- Produces: `bastosFactoryPresets() -> std::vector<Preset>` (`Source/FactoryPresets.h`), consumed only by `BastosUI`'s constructor.

- [ ] **Step 1: Author `Source/FactoryPresets.h`**

```cpp
#pragma once

#include "audioplugins/common/presets/Preset.h"

#include <vector>

// Bastos's factory presets -- authored from scratch for this migration
// (the JUCE-era plugin never had a presets system to convert, unlike Hex's
// hand-converted XML-derived presets). Parameter ids match Source/
// DistrhoPluginInfo.h's symbol strings exactly (atk_gain, atk_sub_count, ...).
inline std::vector<audioplugins::common::presets::Preset> bastosFactoryPresets()
{
    using audioplugins::common::presets::Preset;
    std::vector<Preset> presets;

    {
        // Punchy kick: strong attack transient boost with sub-harmonic
        // reinforcement, minimal sustain shaping.
        Preset p;
        p.name = "Punchy Kick";
        p.pluginId = "com.spellbound.bastos";
        p.schemaVersion = 1;
        p.parameters = {
            {"atk_gain", 8.0f}, {"atk_sub_count", 2.0f}, {"atk_sub_level", 0.6f},
            {"atk_upper_count", 2.0f}, {"atk_upper_level", 0.3f},
            {"sus_gain", -2.0f}, {"sus_sub_count", 1.0f}, {"sus_sub_level", 0.1f},
            {"sus_upper_count", 1.0f}, {"sus_upper_level", 0.0f},
            {"speed", 0.2f}, {"output_gain", 0.0f}, {"mix", 1.0f},
        };
        presets.push_back(std::move(p));
    }
    {
        // Sub reinforcement: heavy sub-harmonic generation on both attack
        // and sustain, gentle gain shaping -- thickens thin kick samples.
        Preset p;
        p.name = "Sub Reinforce";
        p.pluginId = "com.spellbound.bastos";
        p.schemaVersion = 1;
        p.parameters = {
            {"atk_gain", 2.0f}, {"atk_sub_count", 3.0f}, {"atk_sub_level", 0.8f},
            {"atk_upper_count", 1.0f}, {"atk_upper_level", 0.0f},
            {"sus_gain", 1.0f}, {"sus_sub_count", 3.0f}, {"sus_sub_level", 0.7f},
            {"sus_upper_count", 1.0f}, {"sus_upper_level", 0.0f},
            {"speed", 0.5f}, {"output_gain", -1.0f}, {"mix", 1.0f},
        };
        presets.push_back(std::move(p));
    }
    {
        // Snappy transient: fast envelope (low speed), strong upper-harmonic
        // click on attack, sustain pulled down for a tighter, snappier feel.
        Preset p;
        p.name = "Snappy Transient";
        p.pluginId = "com.spellbound.bastos";
        p.schemaVersion = 1;
        p.parameters = {
            {"atk_gain", 10.0f}, {"atk_sub_count", 1.0f}, {"atk_sub_level", 0.2f},
            {"atk_upper_count", 4.0f}, {"atk_upper_level", 0.6f},
            {"sus_gain", -8.0f}, {"sus_sub_count", 1.0f}, {"sus_sub_level", 0.0f},
            {"sus_upper_count", 1.0f}, {"sus_upper_level", 0.0f},
            {"speed", 0.0f}, {"output_gain", -2.0f}, {"mix", 1.0f},
        };
        presets.push_back(std::move(p));
    }

    return presets;
}
```

- [ ] **Step 2: Add the preset bar to `BastosUI`, following Hex's/Tank's presets-panel pattern**

Add to `Source/BastosUI.h`:

```cpp
#include "audioplugins/common/hui/dgl/PresetSelector.h"
#include "audioplugins/common/hui/dgl/Button.h"
#include "audioplugins/common/presets/PresetBrowser.h"
```

and private members:

```cpp
    std::unique_ptr<audioplugins::common::presets::PresetBrowser> presetBrowser_;
    std::unique_ptr<audioplugins::common::hui::dgl::PresetSelector> presetSelector_;
    std::unique_ptr<audioplugins::common::hui::dgl::Button> saveButton_;
    std::unique_ptr<audioplugins::common::hui::dgl::Button> deleteButton_;

    void loadPreset(int index);
```

In `Source/BastosUI.cpp`'s constructor, construct `presetBrowser_` with `bastosFactoryPresets()` and a `~/.config/Bastos/presets/` user directory (matching Hex's `~/.config/Hex/presets/` convention exactly, substituting the plugin name), then `presetSelector_`/`saveButton_`/`deleteButton_` wired the same way Hex's `HexUI.cpp` does: `PresetSelector`'s selection callback calls `presetBrowser_->selectIndex()` then `loadPreset()` (which calls `setParameterValue()` once per parameter, exactly the JUCE-era `PresetManager`'s approach per Hex's `CLAUDE.md`); `saveButton_` opens `openFileBrowser()` (DPF's native save dialog, per `DISTRHO_UI_FILE_BROWSER 1` already set in `DistrhoPluginInfo.h`); `deleteButton_` is disabled whenever the current preset `isFactory`. Grow the canvas to fit a preset bar above the title, matching Hex's exact precedent (canvas grew 220px → 256px for a `kPresetBarH` slot) — Bastos's canvas grows from `460` to `484` (`kPresetBarH = 24`, same value Hex/Tank use).

Update `Source/DistrhoPluginInfo.h`:

```cpp
#define DISTRHO_UI_DEFAULT_HEIGHT 484   // was 460 -- +24px preset bar, matching Hex/Tank's kPresetBarH
```

- [ ] **Step 3: Write a framework-free unit test for the factory presets' shape**

`Tests/test_factorypresets.cpp`:

```cpp
#include "test_runner.h"
#include "../Source/FactoryPresets.h"

int main()
{
    const auto presets = bastosFactoryPresets();
    CHECK(presets.size() == 3);

    for (const auto& p : presets)
    {
        CHECK(p.pluginId == "com.spellbound.bastos");
        CHECK(p.parameters.size() == 13);   // all 13 BASTOS_PARAM_* ids present
    }

    CHECK(presets[0].name == "Punchy Kick");
    CHECK(presets[1].name == "Sub Reinforce");
    CHECK(presets[2].name == "Snappy Transient");

    TEST_SUMMARY();
    return 0;
}
```

Add to `CMakeLists.txt` (after the `test_transientshapercore` block):

```cmake
add_executable(test_factorypresets Tests/test_factorypresets.cpp)
target_include_directories(test_factorypresets PRIVATE Source/ Tests/)
target_link_libraries(test_factorypresets PRIVATE AudioPluginsCommon::presets)
target_compile_features(test_factorypresets PRIVATE cxx_std_20)
add_test(NAME FactoryPresets COMMAND test_factorypresets)
```

- [ ] **Step 4: Build and run the full test suite**

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected: `TransientShaperCore` and `FactoryPresets` both pass (2/2).

- [ ] **Step 5: Commit**

```bash
git add Source/FactoryPresets.h Source/BastosUI.h Source/BastosUI.cpp Source/DistrhoPluginInfo.h Tests/test_factorypresets.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
Add a factory-presets panel to Bastos (new feature)

Bastos never had a presets system in JUCE (getNumPrograms() always
returned 1, no PresetManager existed). Adds 3 hand-authored factory
presets (Punchy Kick, Sub Reinforce, Snappy Transient) via
Source/FactoryPresets.h, backed by AudioPluginsCommon::presets::
PresetBrowser and a PresetSelector/Button bar in BastosUI, following
Hex's/Tank's precedent exactly (including the ~/.config/<Plugin>/presets/
user-preset directory convention and the +24px canvas growth for the bar).

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Validator CI legs (`pluginval` / `clap-validator` / `lv2lint`) + fix findings

**Files:**
- Modify: `.github/workflows/ci.yml`

**Interfaces:** N/A — CI-only change.

- [ ] **Step 1: Append the validator steps to the Linux leg of `.github/workflows/ci.yml`**

Copy Tank's Task 8 Step 1 block verbatim (`AudioPlugins/Tank/docs/superpowers/plans/2026-09-05-tank-dpf-migration.md`'s Task 8), substituting `Bastos`/`bastos` for `Tank`/`tank` throughout (pluginval download+`--validate build/bin/Bastos.vst3`, clap-validator `validate build/bin/Bastos.clap`, lv2lint build + `-s lv2_generate_ttl "https://spellbound.audio/plugins/bastos"`, `libelf-dev` baked in from day one, the DPF-wide `doap:Project` lv2lint finding left `continue-on-error: true`).

- [ ] **Step 2: Run pluginval locally**

```bash
curl -sL -o pluginval_Linux.zip https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Linux.zip
unzip -o pluginval_Linux.zip && chmod +x pluginval
xvfb-run -a ./pluginval --strictness-level 5 --validate build/bin/Bastos.vst3
```

For each distinct finding: `gh issue create --repo TriYop/spellbound-MaxDPS --title "pluginval: <finding>" --body "<output excerpt + root-cause investigation>"`, fix the root cause, verify locally, close referencing the fixing commit. Do not guess findings in advance.

- [ ] **Step 3: Run clap-validator locally**

```bash
curl -sL -o clap-validator.zip https://github.com/free-audio/clap-validator/releases/download/0.4.1/clap-validator-0.4.1-127-g152b982-ubuntu-22.04.zip
unzip -o clap-validator.zip
tar -xzf clap-validator-*-ubuntu-22.04.tar.gz
chmod +x clap-validator
xvfb-run -a ./clap-validator validate build/bin/Bastos.clap
```

Same ticket-per-finding process. Since Bastos reports zero latency and never calls `setLatency()`, the `transport-null`/`transport-fuzz*` failure class Tank hit (which required its third `dpf-clap-latency-deactivate-report.patch`) should not occur — if it does, investigate before assuming the two applied patches are sufficient; do not silently add Tank's third patch without confirming the same call-site issue actually manifests here.

- [ ] **Step 4: Run lv2lint locally**

```bash
git clone --depth 1 https://github.com/sfztools/lv2lint /tmp/lv2lint
meson setup -Donline-tests=disabled -Delf-tests=enabled -Dx11-tests=disabled /tmp/lv2lint/build /tmp/lv2lint
ninja -C /tmp/lv2lint/build
export LV2_PATH="/usr/lib/lv2:$(pwd)/build/bin"
export LD_PRELOAD="/tmp/lv2lint/build/lv2lint.so"
/tmp/lv2lint/build/lv2lint.bin -s lv2_generate_ttl "https://spellbound.audio/plugins/bastos"
```

Same ticket-per-finding process, except the known DPF-wide "Plugin Class" finding (already accepted non-blocking).

- [ ] **Step 5: Commit the CI change (and any fixes as separate commits)**

```bash
git add .github/workflows/ci.yml
git commit -m "Add pluginval/clap-validator/lv2lint Linux CI legs"
```

```bash
git commit -m "Fix <finding>, closes #<issue-number>"   # per fix, if any
```

- [ ] **Step 6: Push and verify with `gh run list`**

```bash
git push -u origin worktree-dpf-stage0
gh run list --repo TriYop/spellbound-MaxDPS --branch worktree-dpf-stage0 --limit 5
```

Expected: the Linux leg is green. If it fails at "Configure Common repo access", that means Task 2 Step 2's `gh secret set` didn't actually take — re-run `gh secret list --repo TriYop/spellbound-MaxDPS` to confirm, don't guess. Do not report CI as passing without checking this output directly.

---

## Task 7: Rewrite `CLAUDE.md` for the real, DPF-based, implemented state

**Files:**
- Modify: `CLAUDE.md`

**Interfaces:** N/A — documentation only.

- [ ] **Step 1: Rewrite `CLAUDE.md` from scratch**

```markdown
# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Spellbound Bastos is a DPF audio **effect** plugin (VST3 / CLAP / LV2) combining a dual-envelope (fast/sustain) transient designer with independent sub-harmonic and upper-harmonic generation per phase, targeting kick drum processing and general low-frequency transient shaping. It is an effect -- audio is modified in-place, not analyzed. There is no bypass parameter and no bandpass/dynamic-EQ mode -- an earlier version of this document described a "BassEnhancer" stage and bypass/bandpass controls that were never actually implemented; ignore any reference to those elsewhere, they do not exist in this codebase.

**Current state: migrated off JUCE onto DPF.** `Source/DSP/TransientShaperCore.h/.cpp` is a framework-free port of the original JUCE-dependent `TransientShaper` (replacing `juce::dsp::IIR::Filter`/`juce::AudioBuffer`/`juce::Decibels` with a hand-rolled RBJ biquad and raw channel pointers), unit-tested via CTest and verified against the JUCE original via a golden-vector capture. `Source/BastosPluginAdapter.{h,cpp}` wires all 13 host parameters and replicates the JUCE-era processBlock()'s dry/wet mix, output gain, and peak metering. `Source/BastosUI.{h,cpp}` has the ported editor (13 rotary knobs across Attack/Sustain/Global groups, 2 VU meters) plus a **new** factory-presets panel (Bastos never had one in JUCE) backed by `Source/FactoryPresets.h`'s 3 presets and `AudioPlugins/Common`'s `PresetBrowser`/`PresetSelector`/`Button`. The JUCE-era `PluginProcessor`/`PluginEditor`/original `TransientShaper` are preserved unchanged under `Source/_juce_reference/` as the porting reference. Rebranded from the legacy `YvanJanet`/`com.yvanjanet.bastos` identity to `Spellbound`/`com.spellbound.bastos`, matching every other migrated plugin. See `docs/superpowers/plans/2026-09-06-maxdps-dpf-migration.md` for the full task-by-task migration record.

## Build Commands

### Linux prerequisites (one-time)

\`\`\`bash
sudo apt install cmake ninja-build build-essential git \
    libasound2-dev libjack-jackd2-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev \
    libfreetype-dev libfontconfig1-dev \
    libglu1-mesa-dev libwebkit2gtk-4.1-dev
\`\`\`

### Configure / build / run

\`\`\`bash
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
\`\`\`

### Release packaging

\`\`\`bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
cd build-release && cpack
\`\`\`

## Architecture

### Source layout

\`\`\`
Source/
  DistrhoPluginInfo.h        -- DPF metadata, BastosParameters enum, BASTOS_PARAM_* ranges/defaults
  BastosPluginAdapter.h/.cpp -- DPF Plugin: parameters, dry/wet mix, output gain, peak metering
  BastosUI.h/.cpp            -- DPF UI: 13 RotaryKnobs, 2 VuMeters, presets bar
  FactoryPresets.h           -- 3 hand-authored factory presets
  DSP/
    TransientShaperCore.h/.cpp -- framework-free dual-envelope transient designer (unit-tested)
  _juce_reference/            -- pre-migration JUCE code, preserved as the porting reference
\`\`\`

### Signal flow

\`\`\`
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
  \`- measure output peak (MeterTransport)
\`\`\`

### Parameters

| Host symbol | Range | Default | Description |
|---|---|---|---|
| \`atk_gain\` | -12 to 12 dB | 0 | gain applied during transient attack |
| \`atk_sub_count\` | 1-3 | 1 | sub-harmonic drive multiplier during attack |
| \`atk_sub_level\` | 0-1 | 0 | sub-harmonic blend level during attack |
| \`atk_upper_count\` | 1-5 | 1 | upper-harmonic drive multiplier during attack |
| \`atk_upper_level\` | 0-1 | 0 | upper-harmonic blend level during attack |
| \`sus_gain\` | -12 to 12 dB | 0 | gain applied during transient sustain/decay |
| \`sus_sub_count\` | 1-3 | 1 | sub-harmonic drive multiplier during sustain |
| \`sus_sub_level\` | 0-1 | 0 | sub-harmonic blend level during sustain |
| \`sus_upper_count\` | 1-5 | 1 | upper-harmonic drive multiplier during sustain |
| \`sus_upper_level\` | 0-1 | 0 | upper-harmonic blend level during sustain |
| \`speed\` | 0-1 | 0 | envelope follower speed (fast<->slow time constants) |
| \`output_gain\` | -12 to 12 dB | 0 | output trim |
| \`mix\` | 0-1 | 1 | global dry/wet |

No bypass parameter exists (there never was one, even pre-migration).

### Key design constraints

- **DSP is framework-free and unit-tested independently of DPF** (\`Source/DSP/TransientShaperCore.h/.cpp\` + \`Tests/test_transientshapercore.cpp\`, CTest, verified against the original JUCE implementation via a golden-vector capture) -- \`BastosPluginAdapter\`/\`BastosUI\` are thin adapters with no DSP logic of their own.
- **In-place processing**: \`run()\` modifies the output buffer directly; \`TransientShaperCore\` owns no per-block scratch beyond its 2-channel envelope/filter state.
- **Peak meters are direct-access, not DPF parameters**: \`DISTRHO_PLUGIN_WANT_DIRECT_ACCESS\` + \`getPluginInstancePointer()\`, same idiom as every other plugin in this workspace -- clap-validator rejects host-visible, audio-reactive output parameters regardless of hints.
- **Zero latency**: \`TransientShaperCore\` has no delay lines; \`DISTRHO_PLUGIN_WANT_LATENCY 0\`.
```

- [ ] **Step 2: Commit**

```bash
git add CLAUDE.md
git commit -m "Rewrite CLAUDE.md for Bastos's DPF migration, correcting the stale pre-migration parameter/architecture description"
```

---

## Task 8: Final verification, manual host pass, and wrap-up

**Files:** none (verification + a GitHub issue + PR).

**Interfaces:** N/A.

- [ ] **Step 1: Full local verification pass from a clean build**

```bash
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected: clean configure/build, 2/2 tests passing (`TransientShaperCore`, `FactoryPresets`).

- [ ] **Step 2: Manual host pass in Carla covering knobs, metering, and presets together**

```bash
carla &
```

Load `build/bin/Bastos.vst3` (or the CLAP) with a kick-drum sample looping through it, and:
1. Sweep each of the 13 knobs and confirm audible/visible effect (attack knobs affect only the transient onset, sustain knobs the decay, `speed` changes envelope responsiveness, `mix` blends dry/wet, `output_gain` trims level).
2. Confirm the IN/OUT VU meters track the kick's level in real time.
3. Select each of the 3 factory presets (Punchy Kick, Sub Reinforce, Snappy Transient) and confirm all 13 knobs visibly jump to that preset's values.
4. Save a modified knob set as a new user preset via the SAVE button's native file dialog; confirm it appears in the dropdown with DELETE enabled; delete it and confirm it disappears.

- [ ] **Step 3: File the tracked verification issue**

```bash
gh issue create --repo TriYop/spellbound-MaxDPS \
  --title "Manual host verification: knobs + metering + presets (DPF migration)" \
  --body "$(cat <<'EOF'
Tracks the final manual verification pass for Bastos's DPF migration, per
docs/superpowers/plans/2026-09-06-maxdps-dpf-migration.md's Task 8.

- [ ] All 13 knobs produce an audible/visible effect when swept
- [ ] IN/OUT VU meters track a real kick sample's level
- [ ] All 3 factory presets (Punchy Kick, Sub Reinforce, Snappy Transient) recall correctly in Carla
- [ ] Saving a user preset works via the native file dialog and appears with DELETE enabled
- [ ] Deleting a user preset removes it from the dropdown
EOF
)"
```

Close it once Step 2's checklist is confirmed.

- [ ] **Step 4: Open the pull request**

```bash
git push -u origin worktree-dpf-stage0
gh pr create --repo TriYop/spellbound-MaxDPS \
  --title "Migrate Bastos off JUCE onto DPF, extract framework-free DSP, add presets panel" \
  --body "$(cat <<'EOF'
## Summary
- Extracts TransientShaper (the only DSP class, previously embedding
  juce::dsp::ProcessSpec/IIR::Filter/AudioBuffer/Decibels directly) into
  a framework-free TransientShaperCore, verified against the JUCE
  original via a golden-vector capture -- the one migration in this
  workspace so far whose DSP wasn't already framework-free.
- Rewrites the build off JUCE + clap-juce-extensions onto DPF (pinned
  commit + 2 upstream patches) and AudioPlugins/Common v0.3.0.
- Rebrands from legacy YvanJanet/Yjnt identity to Spellbound/Spbd,
  matching every other migrated plugin.
- Ports the 13-knob + 2-meter editor onto Common's RotaryKnob/VuMeter.
- Adds a factory-presets panel -- a new feature, since the JUCE-era
  plugin never had one.
- Corrects CLAUDE.md, which pre-migration described a fictional
  BassEnhancer/bypass/bandpass design never actually implemented.
- Adds pluginval/clap-validator/lv2lint Linux CI legs.

## Test plan
- [x] \`ctest --test-dir build --output-on-failure\` -- 2/2 passing
- [x] Manual host verification in Carla (knobs, metering, presets) -- see linked issue
- [ ] CI green on this PR (\`gh run list --repo TriYop/spellbound-MaxDPS\`)
EOF
)"
```

- [ ] **Step 5: Verify CI on the PR before considering this plan complete**

```bash
gh run list --repo TriYop/spellbound-MaxDPS --branch worktree-dpf-stage0 --limit 5
```

Do not report the migration as complete without this coming back green on the Linux leg (Windows/macOS remain `continue-on-error` and are not a completion criterion, per Global Constraints). If it's red at "Configure Common repo access", re-check `gh secret list --repo TriYop/spellbound-MaxDPS` from Task 2 Step 2 before assuming a new bug.
