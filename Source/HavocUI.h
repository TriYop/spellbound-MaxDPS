#pragma once

#include "DistrhoUI.hpp"
#include "HavocPluginAdapter.h"
#include "HavocLayout.h"

#include "audioplugins/common/hui/dgl/MasterStrip.h"
#include "audioplugins/common/hui/dgl/PluginHeader.h"
#include "audioplugins/common/hui/dgl/RotaryKnob.h"
#include "audioplugins/common/presets/PresetBrowser.h"

#include <array>
#include <memory>
#include <vector>

START_NAMESPACE_DISTRHO

/**
   DPF UI adapter for Spellbound Havoc.

   Geometry comes from the framework-free Source/HavocLayout.h. On screen:
   Common's shared PluginHeader on top (preset bar, zoom pills, "SPELLBOUND
   HAVOC" title and the "Transient Devastation" hook), three titled
   ATTACK/SUSTAIN/GLOBAL group panels (drawGroupPanel, each in its accent)
   holding 12 labelled RotaryKnobs, and Common's MasterStrip on the right:
   IN/OUT VuMeters plus the vertical Output Gain fader. IN/OUT levels are
   polled every uiIdle() tick via DISTRHO_PLUGIN_WANT_DIRECT_ACCESS +
   getPluginInstancePointer(), same idiom as Hex's meters -- not a DPF
   parameter.

   Presets: PresetBrowser + Havoc's 3 compiled-in factory presets
   (Source/FactoryPresets.h), reached through the header's selector and
   SAVE/DELETE buttons.
 */
class HavocUI : public UI
{
public:
    HavocUI();

protected:
    void parameterChanged(uint32_t index, float value) override;
    void onNanoDisplay() override;
    void uiIdle() override;
    void stateChanged(const char* key, const char* value) override;

    // Called by DPF after the user picks a file (or cancels) in the native
    // save dialog SAVE's onClick opens.
    void uiFileBrowserSelected(const char* filename) override;

private:
    static constexpr uint32_t kNumParams = havoc::layout::kParamCount;

    const havoc::layout::UiLayout layout_;

    // Indexed by parameter index; knobs_[kParameterOutputGain] stays null
    // (Output Gain is the master strip's fader, see isFaderParam()).
    std::array<std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob>, kNumParams> knobs_;
    audioplugins::common::hui::dgl::MasterStrip master_;
    // Non-owning: the strip's Output Gain fader (owned by master_).
    audioplugins::common::hui::dgl::LabeledFader* const fader_;

    // Raw, non-owning: HavocUI does not own the DSP instance's lifetime,
    // DPF does. Captured once in the constructor via
    // getPluginInstancePointer() (DISTRHO_PLUGIN_WANT_DIRECT_ACCESS).
    HavocPluginAdapter* const pluginPtr_;

    // PresetBrowser is not a DGL widget, so it's a plain member.
    audioplugins::common::presets::PresetBrowser presetBrowser_;
    // Shared header: owns the zoom pills, preset dropdown and SAVE/DELETE
    // buttons (pills constructed before the dropdown so it draws over them).
    // Declared after every other widget so the open dropdown overlays them too.
    audioplugins::common::hui::dgl::PluginHeader header_;

    // Programmatic (no host echo) value set for any parameter: the knob, or
    // the fader for Output Gain.
    void setControlValue(uint32_t index, float value);
    float controlValue(uint32_t index) const;
    void applyPreset(const audioplugins::common::presets::Preset& preset);
    std::vector<audioplugins::common::presets::ParameterValue> captureCurrentParameters() const;
    // Pushes presetBrowser_'s current entries/index into the header's
    // selector and updates its DELETE button's enabled state.
    void refreshPresetControls();

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HavocUI)
};

END_NAMESPACE_DISTRHO
