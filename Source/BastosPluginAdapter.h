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
