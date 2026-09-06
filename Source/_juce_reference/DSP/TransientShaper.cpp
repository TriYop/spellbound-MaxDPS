#include "TransientShaper.h"
#include <cmath>
#include <algorithm>

void TransientShaper::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate_ = spec.sampleRate;
    lastSpeed_  = -1.f;

    const juce::dsp::ProcessSpec monoSpec { spec.sampleRate, spec.maximumBlockSize, 1 };
    for (auto& c : ch_)
    {
        c.subInputLp .prepare (monoSpec);
        c.subOutputLp.prepare (monoSpec);
    }

    updateFilterCoeffs();
    reset();
}

void TransientShaper::reset()
{
    for (auto& c : ch_)
    {
        c.envFast = 0.f;
        c.envSlow = 0.f;
        c.subInputLp .reset();
        c.subOutputLp.reset();
    }
}

void TransientShaper::updateEnvCoeffs (float speedFrac)
{
    if (std::abs (speedFrac - lastSpeed_) < 1e-4f) return;
    lastSpeed_ = speedFrac;

    // fast: 1–5 ms; slow: 50–200 ms
    const float sr     = static_cast<float> (sampleRate_);
    const float fastMs = 1.f + speedFrac * 4.f;
    const float slowMs = 50.f + speedFrac * 150.f;
    alphaFast_ = std::exp (-1000.f / (fastMs * sr));
    alphaSlow_ = std::exp (-1000.f / (slowMs * sr));
}

void TransientShaper::updateFilterCoeffs()
{
    const auto inCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowPass (
        sampleRate_, kSubInputCutoffHz);
    const auto outCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (
        sampleRate_, kSubOutputCutoffHz);

    for (auto& c : ch_)
    {
        *c.subInputLp .coefficients = *inCoeffs;
        *c.subOutputLp.coefficients = *outCoeffs;
    }
}

void TransientShaper::process (juce::AudioBuffer<float>& buffer,
                                float atkGainDb,
                                int   atkSubCount,  float atkSubLevel,
                                int   atkUpperCount, float atkUpperLevel,
                                float susGainDb,
                                int   susSubCount,  float susSubLevel,
                                int   susUpperCount, float susUpperLevel,
                                float speedFrac,
                                bool  enabled)
{
    if (!enabled) return;

    const int nSamples  = buffer.getNumSamples();
    const int nChannels = std::min (buffer.getNumChannels(), 2);

    updateEnvCoeffs (speedFrac);

    // Pre-compute linear gains and drives (block-rate, not per-sample)
    const float atkGainLin   = juce::Decibels::decibelsToGain (atkGainDb);
    const float susGainLin   = juce::Decibels::decibelsToGain (susGainDb);
    const float atkSubDrive  = 3.f * static_cast<float> (atkSubCount);
    const float susSubDrive  = 3.f * static_cast<float> (susSubCount);
    const float atkUpperDrive = 2.f * static_cast<float> (atkUpperCount);
    const float susUpperDrive = 2.f * static_cast<float> (susUpperCount);

    for (int ch = 0; ch < nChannels; ++ch)
    {
        auto& state = ch_[static_cast<size_t> (ch)];
        float* buf  = buffer.getWritePointer (ch);

        for (int i = 0; i < nSamples; ++i)
        {
            const float x    = buf[i];
            const float rect = std::abs (x);

            // Dual-envelope followers
            state.envFast = alphaFast_ * state.envFast + (1.f - alphaFast_) * rect;
            state.envSlow = alphaSlow_ * state.envSlow + (1.f - alphaSlow_) * rect;

            // Normalised transient in [-1, +1]; near 0 → neither phase dominates
            const float envSum = state.envFast + state.envSlow + 1e-7f;
            const float t      = (state.envFast - state.envSlow) / envSum;
            const float atkW   = std::max (0.f, t);
            const float susW   = std::max (0.f, -t);

            // Interpolate all parameters from envelope weights
            const float gain       = 1.f + atkW * (atkGainLin - 1.f) + susW * (susGainLin - 1.f);
            const float subDrive   = atkW * atkSubDrive   + susW * susSubDrive;
            const float subLevel   = atkW * atkSubLevel   + susW * susSubLevel;
            const float upperDrive = atkW * atkUpperDrive + susW * susUpperDrive;
            const float upperLevel = atkW * atkUpperLevel + susW * susUpperLevel;

            // Sub-harmonics: saturate bass-isolated signal, keep content below 300 Hz
            const float bass    = state.subInputLp.processSample (x);
            const float subSat  = subDrive > 0.01f ? std::tanh (bass * subDrive) : bass;
            const float subHarm = state.subOutputLp.processSample (subSat - bass);

            // Upper harmonics: saturate full-band signal, remove fundamental
            const float upperSat  = upperDrive > 0.01f ? std::tanh (x * upperDrive) : x;
            const float upperHarm = upperSat - x;

            buf[i] = gain * x + subLevel * subHarm + upperLevel * upperHarm;
        }
    }
}
