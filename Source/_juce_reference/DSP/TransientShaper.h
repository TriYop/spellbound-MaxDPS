#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

// Unified harmonic transient designer.
//
// Detects transient events via a dual-envelope follower (fast − slow).
// Positive transient → attack phase; negative → sustain phase.
// Each phase independently controls:
//   - gain
//   - sub-harmonic content (bass waveshaping, count 1-3 → drive)
//   - upper-harmonic content (full-band waveshaping, count 1-5 → drive)
//
// Sub-harmonics:   LP(x, 150 Hz) → tanh saturation − dry → LP(300 Hz) → blend
// Upper harmonics: tanh(x * drive) − x → blend
//
// All gain and blend values are derived from the smoothed envelope weights,
// so no additional per-sample smoothing is needed to prevent clicks.
class TransientShaper
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);

    void process (juce::AudioBuffer<float>& buffer,
                  float atkGainDb,
                  int   atkSubCount,  float atkSubLevel,
                  int   atkUpperCount, float atkUpperLevel,
                  float susGainDb,
                  int   susSubCount,  float susSubLevel,
                  int   susUpperCount, float susUpperLevel,
                  float speedFrac,
                  bool  enabled);

    void reset();

private:
    struct ChannelState
    {
        float envFast { 0.f };
        float envSlow { 0.f };
        juce::dsp::IIR::Filter<float> subInputLp;   // LP at 150 Hz — isolates bass for sub generation
        juce::dsp::IIR::Filter<float> subOutputLp;  // LP at 300 Hz — keeps sub harmonics in bass range
    };

    std::array<ChannelState, 2> ch_;

    double sampleRate_ { 44100.0 };
    float  alphaFast_  { 0.f };
    float  alphaSlow_  { 0.f };
    float  lastSpeed_  { -1.f };

    void updateEnvCoeffs (float speedFrac);
    void updateFilterCoeffs();

    static constexpr float kSubInputCutoffHz  = 150.f;
    static constexpr float kSubOutputCutoffHz = 300.f;
};
