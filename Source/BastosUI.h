#pragma once

#include "DistrhoUI.hpp"
#include "BastosPluginAdapter.h"

#include "audioplugins/common/hui/dgl/RotaryKnob.h"
#include "audioplugins/common/hui/dgl/VuMeter.h"
#include "audioplugins/common/hui/dgl/PresetSelector.h"
#include "audioplugins/common/hui/dgl/Button.h"
#include "audioplugins/common/presets/PresetBrowser.h"

#include <array>
#include <memory>
#include <vector>

START_NAMESPACE_DISTRHO

/**
   DPF UI adapter for Spellbound Bastos.

   13 RotaryKnobs across three groups (Attack/Sustain/Global) plus 2
   VuMeters (IN/OUT), laid out to match the JUCE-era 660x460 editor's
   three-column layout (see Source/_juce_reference/PluginEditor.cpp).
   IN/OUT levels are polled every uiIdle() tick via
   DISTRHO_PLUGIN_WANT_DIRECT_ACCESS + getPluginInstancePointer(), same
   idiom as Hex's meters -- not a DPF parameter.

   Also has a presets panel (PresetSelector dropdown + SAVE/DELETE buttons)
   above the knob panel, backed by AudioPluginsCommon::presets::
   PresetBrowser and Bastos's 3 compiled-in factory presets
   (Source/FactoryPresets.h) -- new to Bastos (the JUCE-era plugin never had
   a presets system), following Hex's HexUI.h/.cpp precedent exactly.
 */
class BastosUI : public UI
{
public:
    BastosUI();

protected:
    void parameterChanged(uint32_t index, float value) override;
    void onNanoDisplay() override;
    void uiIdle() override;

    // Called by DPF after the user picks a file (or cancels) in the native
    // save dialog SAVE's onClick opens.
    void uiFileBrowserSelected(const char* filename) override;

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

    // PresetBrowser is not a DGL widget (no UI base-class dependency), so
    // it's a plain member, not heap-owned like the widgets below.
    audioplugins::common::presets::PresetBrowser presetBrowser_;
    std::unique_ptr<audioplugins::common::hui::dgl::PresetSelector> presetSelector_;
    std::unique_ptr<audioplugins::common::hui::dgl::Button> saveButton_;
    std::unique_ptr<audioplugins::common::hui::dgl::Button> deleteButton_;

    void setKnobValue(uint32_t index, float value);
    void applyPreset(const audioplugins::common::presets::Preset& preset);
    std::vector<audioplugins::common::presets::ParameterValue> captureCurrentParameters() const;
    // Pushes presetBrowser_'s current entries/index into presetSelector_ and
    // updates deleteButton_'s enabled state. Called after construction and
    // after any load/save/delete.
    void refreshPresetControls();

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BastosUI)
};

END_NAMESPACE_DISTRHO
