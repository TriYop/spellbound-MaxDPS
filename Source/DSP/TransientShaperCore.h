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
