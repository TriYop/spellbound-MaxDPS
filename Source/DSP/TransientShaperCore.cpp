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
