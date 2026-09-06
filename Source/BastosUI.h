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

    // Raw, non-owning: BastosUI does not own the DSP instance's lifetime,
    // DPF does. Captured once in the constructor via
    // getPluginInstancePointer() (DISTRHO_PLUGIN_WANT_DIRECT_ACCESS) --
    // same idiom as Hex's HexUI::fPluginPtr.
    BastosPluginAdapter* const pluginPtr_;

    void setKnobValue(uint32_t index, float value);

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BastosUI)
};

END_NAMESPACE_DISTRHO
