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

    // Invariant 3: golden-vector regression against the real JUCE-era
    // TransientShaper's output for a non-trivial input, captured via
    // Tests/generate_golden_vectors.cpp (see Task 1 Step 5 in the migration
    // plan for how these numbers were obtained -- they are NOT hand-derived).
    {
        static constexpr float kGoldenLeft[32] = {
            0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f,
            1.949983120f, -1.949782848f, 1.949581742f, -1.949370146f,
            1.949178934f, -1.948945880f, 1.948782802f, -1.948507309f,
            0.000057778f, 0.000078225f, 0.000101863f, 0.000128346f,
            0.000157341f, 0.000188534f, 0.000221628f, 0.000256337f,
            0.000292396f, 0.000329551f, 0.000367563f, 0.000406207f,
            0.000445273f, 0.000484561f, 0.000523887f, 0.000563077f,
            0.000601969f, 0.000640414f, 0.000678271f, 0.000715412f,
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

    TEST_SUMMARY();
    return 0;
}
