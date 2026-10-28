#include "BastosUI.h"
#include "FactoryPresets.h"

#include "audioplugins/common/hui/dgl/GroupPanel.h"

#include <cmath>
#include <cstdlib>
#include <string>

START_NAMESPACE_DISTRHO

namespace {

namespace hui = audioplugins::common::hui;
namespace bl = bastos::layout;

// BastosLayout.h mirrors the parameter enum without including this header.
static_assert(bl::kAtkGain == kParameterAtkGain && bl::kAtkSubCount == kParameterAtkSubCount &&
              bl::kAtkSubLevel == kParameterAtkSubLevel && bl::kAtkUpperCount == kParameterAtkUpperCount &&
              bl::kAtkUpperLevel == kParameterAtkUpperLevel && bl::kSusGain == kParameterSusGain &&
              bl::kSusSubCount == kParameterSusSubCount && bl::kSusSubLevel == kParameterSusSubLevel &&
              bl::kSusUpperCount == kParameterSusUpperCount && bl::kSusUpperLevel == kParameterSusUpperLevel &&
              bl::kSpeed == kParameterSpeed && bl::kOutputGain == kParameterOutputGain &&
              bl::kMix == kParameterMix && bl::kParamCount == kParameterCount,
              "BastosLayout.h parameter indices must match the kParameter* enum");
static_assert(bl::isFaderParam(kParameterOutputGain), "Output Gain is the master fader");

const hui::Theme& kTheme = hui::defaultTheme();

// Per-section accents (blue attack / teal sustain / gold global, from the
// JUCE-era kAtkAccent/kSusAccent/kGlobAccent): group titles and knob arcs.
constexpr hui::Colour kAtkAccent { 0x44, 0xaa, 0xee, 0xff };
constexpr hui::Colour kSusAccent { 0x44, 0xdd, 0xaa, 0xff };
constexpr hui::Colour kGlobAccent{ 0xaa, 0xaa, 0x66, 0xff };

constexpr float kLabelFontSize = 11.0f;

DGL_NAMESPACE::Color toDglColor(const hui::Colour& c) noexcept
{
    return DGL_NAMESPACE::Color(static_cast<int>(c.r), static_cast<int>(c.g),
                                static_cast<int>(c.b), static_cast<float>(c.a) / 255.f);
}

// One entry per parameter index. Output Gain (the fader) has no knob: its
// range and label feed the master strip's fader, its accent is unused.
struct ParamSpec { const char* id; float min, max, def; const hui::Colour* accent; const char* label; };

constexpr ParamSpec kParams[kParameterCount] = {
    {"atk_gain",        BASTOS_PARAM_ATK_GAIN_MIN,        BASTOS_PARAM_ATK_GAIN_MAX,        BASTOS_PARAM_ATK_GAIN_DEFAULT,        &kAtkAccent,  "GAIN"},
    {"atk_sub_count",   BASTOS_PARAM_ATK_SUB_COUNT_MIN,   BASTOS_PARAM_ATK_SUB_COUNT_MAX,   BASTOS_PARAM_ATK_SUB_COUNT_DEFAULT,   &kAtkAccent,  "SUB #"},
    {"atk_sub_level",   BASTOS_PARAM_ATK_SUB_LEVEL_MIN,   BASTOS_PARAM_ATK_SUB_LEVEL_MAX,   BASTOS_PARAM_ATK_SUB_LEVEL_DEFAULT,   &kAtkAccent,  "SUB LVL"},
    {"atk_upper_count", BASTOS_PARAM_ATK_UPPER_COUNT_MIN, BASTOS_PARAM_ATK_UPPER_COUNT_MAX, BASTOS_PARAM_ATK_UPPER_COUNT_DEFAULT, &kAtkAccent,  "UPPER #"},
    {"atk_upper_level", BASTOS_PARAM_ATK_UPPER_LEVEL_MIN, BASTOS_PARAM_ATK_UPPER_LEVEL_MAX, BASTOS_PARAM_ATK_UPPER_LEVEL_DEFAULT, &kAtkAccent,  "UPPER LVL"},
    {"sus_gain",        BASTOS_PARAM_SUS_GAIN_MIN,        BASTOS_PARAM_SUS_GAIN_MAX,        BASTOS_PARAM_SUS_GAIN_DEFAULT,        &kSusAccent,  "GAIN"},
    {"sus_sub_count",   BASTOS_PARAM_SUS_SUB_COUNT_MIN,   BASTOS_PARAM_SUS_SUB_COUNT_MAX,   BASTOS_PARAM_SUS_SUB_COUNT_DEFAULT,   &kSusAccent,  "SUB #"},
    {"sus_sub_level",   BASTOS_PARAM_SUS_SUB_LEVEL_MIN,   BASTOS_PARAM_SUS_SUB_LEVEL_MAX,   BASTOS_PARAM_SUS_SUB_LEVEL_DEFAULT,   &kSusAccent,  "SUB LVL"},
    {"sus_upper_count", BASTOS_PARAM_SUS_UPPER_COUNT_MIN, BASTOS_PARAM_SUS_UPPER_COUNT_MAX, BASTOS_PARAM_SUS_UPPER_COUNT_DEFAULT, &kSusAccent,  "UPPER #"},
    {"sus_upper_level", BASTOS_PARAM_SUS_UPPER_LEVEL_MIN, BASTOS_PARAM_SUS_UPPER_LEVEL_MAX, BASTOS_PARAM_SUS_UPPER_LEVEL_DEFAULT, &kSusAccent,  "UPPER LVL"},
    {"speed",           BASTOS_PARAM_SPEED_MIN,           BASTOS_PARAM_SPEED_MAX,           BASTOS_PARAM_SPEED_DEFAULT,           &kGlobAccent, "SPEED"},
    {"output_gain",     BASTOS_PARAM_OUTPUT_GAIN_MIN,     BASTOS_PARAM_OUTPUT_GAIN_MAX,     BASTOS_PARAM_OUTPUT_GAIN_DEFAULT,     &kGlobAccent, "VOL"},
    {"mix",             BASTOS_PARAM_MIX_MIN,             BASTOS_PARAM_MIX_MAX,             BASTOS_PARAM_MIX_DEFAULT,             &kGlobAccent, "MIX"},
};

int px(float v) noexcept { return static_cast<int>(std::lround(v)); }
uint upx(float v) noexcept { return static_cast<uint>(std::lround(v)); }

// Gesture bracketing follows DPF's own idiom (examples/CairoUI: drag
// started/finished -> editParameter, value changed -> setParameterValue).
template <class Control>
void bindToParameter(BastosUI& ui, Control& control, const uint32_t paramIndex)
{
    control.onDragStateChanged = [&ui, paramIndex](const bool started) { ui.editParameter(paramIndex, started); };
    control.onValueChanged = [&ui, paramIndex](const float value) { ui.setParameterValue(paramIndex, value); };
}

std::unique_ptr<hui::dgl::RotaryKnob> makeKnob(BastosUI& ui, const bl::KnobSlot& slot, const uint32_t paramIndex)
{
    const ParamSpec& spec = kParams[paramIndex];
    auto knob = std::make_unique<hui::dgl::RotaryKnob>(&ui);
    knob->setRange(spec.min, spec.max);
    knob->setDefaultValue(spec.def);
    knob->setValue(spec.def);
    knob->setPalette(hui::dgl::knobPaletteFor(*spec.accent));
    knob->setAbsolutePos(px(slot.body.x), px(slot.body.y));
    knob->setSize(upx(slot.body.w), upx(slot.body.h));
    bindToParameter(ui, *knob, paramIndex);
    return knob;
}

// Every knob, indexed by parameter index; the fader's slot stays null.
std::array<std::unique_ptr<hui::dgl::RotaryKnob>, kParameterCount> makeKnobs(BastosUI& ui, const bl::UiLayout& layout)
{
    std::array<std::unique_ptr<hui::dgl::RotaryKnob>, kParameterCount> knobs;
    for (uint32_t i = 0; i < kParameterCount; ++i)
        if (!bl::isFaderParam(i))
            knobs[i] = makeKnob(ui, layout.knob[i], i);
    return knobs;
}

// Linux-only for now, matching Hex's hexUserPresetsDirectory() precedent
// (~/.config/<Name>/presets).
std::string bastosUserPresetsDirectory()
{
    const char* home = std::getenv("HOME");
    if (home == nullptr)
        return "/tmp/Bastos/presets"; // extremely unlikely fallback, still functional
    return std::string(home) + "/.config/Bastos/presets";
}

} // namespace

BastosUI::BastosUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true),
      layout_(bl::computeLayout()),
      // Widgets are built in the initialiser list, before header_ (the last
      // member), so an open preset dropdown draws over them.
      knobs_(makeKnobs(*this, layout_)),
      master_(this, /*withFader=*/true),
      fader_(master_.fader()),
      pluginPtr_(static_cast<BastosPluginAdapter*>(getPluginInstancePointer())),
      presetBrowser_(bastosFactoryPresets(), bastosUserPresetsDirectory(), "com.spellbound.bastos"),
      header_(this, "SPELLBOUND HAVOC", DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT,
              [this](const char* key, const char* value) { setState(key, value); })
{
    loadSharedResources();
    header_.setTagline("Transient Devastation");
    DISTRHO_SAFE_ASSERT(header_.bandHeight() == layout_.header.bandHeight);

    master_.layout(layout_.master);
    // The strip's meters show this plugin's input and output peaks, not a
    // stereo pair.
    master_.setMeterLabels("IN", "OUT");
    const ParamSpec& out = kParams[kParameterOutputGain];
    fader_->setRange(out.min, out.max);
    fader_->setDefaultValue(out.def);
    fader_->setValue(out.def);
    fader_->setLabel(out.label);
    bindToParameter(*this, *fader_, kParameterOutputGain);

    header_.presets().onIndexSelected = [this](const int index)
    {
        if (const auto* preset = presetBrowser_.selectIndex(index))
        {
            applyPreset(*preset);
            refreshPresetControls();
        }
    };

    header_.deleteButton().onClick = [this]()
    {
        if (presetBrowser_.deleteCurrent())
            refreshPresetControls();
    };

    header_.saveButton().onClick = [this]()
    {
        const std::string startDir = bastosUserPresetsDirectory();
        FileBrowserOptions options;
        options.saving = true;
        options.defaultName = "New Preset.xml";
        options.title = "Save Bastos Preset";
        options.startDir = startDir.c_str();
        openFileBrowser(options);
    };

    refreshPresetControls();
}

void BastosUI::stateChanged(const char* key, const char* value)
{
    header_.zoom().onStateChanged(key, value);
}

void BastosUI::uiFileBrowserSelected(const char* filename)
{
    if (filename == nullptr)
        return; // user cancelled the dialog

    std::string path(filename);
    const size_t slash = path.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos)
        base = base.substr(0, dot);

    if (presetBrowser_.saveAs(base, captureCurrentParameters()))
        refreshPresetControls();
}

void BastosUI::setControlValue(const uint32_t index, const float value)
{
    // setValue() on either widget deliberately does not fire onValueChanged,
    // so host automation / preset recall cannot loop back out to the host.
    if (index >= kNumParams)
        return;
    if (bl::isFaderParam(index))
        fader_->setValue(value);
    else
        knobs_[index]->setValue(value);
}

float BastosUI::controlValue(const uint32_t index) const
{
    return bl::isFaderParam(index) ? fader_->getValue() : knobs_[index]->getValue();
}

void BastosUI::applyPreset(const audioplugins::common::presets::Preset& preset)
{
    for (const auto& pv : preset.parameters)
    {
        for (uint32_t i = 0; i < kNumParams; ++i)
        {
            if (pv.id == kParams[i].id)
            {
                setControlValue(i, pv.value);
                editParameter(i, true);
                setParameterValue(i, pv.value);
                editParameter(i, false);
                break;
            }
        }
        // Unknown id (forward-compatible with a future schema addition) --
        // ignored if no entry matched.
    }
}

std::vector<audioplugins::common::presets::ParameterValue> BastosUI::captureCurrentParameters() const
{
    std::vector<audioplugins::common::presets::ParameterValue> parameters;
    parameters.reserve(kNumParams);
    for (uint32_t i = 0; i < kNumParams; ++i)
        parameters.push_back({kParams[i].id, controlValue(i)});
    return parameters;
}

void BastosUI::refreshPresetControls()
{
    const auto entries = presetBrowser_.getEntries();
    header_.presets().setEntries(entries);
    header_.presets().setCurrentIndex(presetBrowser_.getCurrentIndex());

    const int idx = presetBrowser_.getCurrentIndex();
    const bool isFactory = (idx >= 0 && static_cast<size_t>(idx) < entries.size())
                                ? entries[static_cast<size_t>(idx)].isFactory
                                : true;
    header_.deleteButton().setEnabled(!isFactory);
}

void BastosUI::parameterChanged(const uint32_t index, const float value)
{
    setControlValue(index, value);
}

void BastosUI::uiIdle()
{
    header_.zoom().onIdle();

    // Direct-access read of the DSP instance's atomically-published peak
    // meters -- see DistrhoPluginInfo.h's DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
    // comment for why this isn't a DPF parameter. Guarded as cheap insurance.
    if (pluginPtr_ == nullptr)
        return;

    master_.left().pushLevel(pluginPtr_->getInputLevel());
    master_.right().pushLevel(pluginPtr_->getOutputLevel());
}

void BastosUI::onNanoDisplay()
{
    beginPath();
    rect(0, 0, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillColor(toDglColor(kTheme.windowBackground));
    fill();
    closePath();

    hui::dgl::drawGroupPanel(*this, layout_.atk, "ATTACK", &kAtkAccent);
    hui::dgl::drawGroupPanel(*this, layout_.sus, "SUSTAIN", &kSusAccent);
    hui::dgl::drawGroupPanel(*this, layout_.glob, "GLOBAL", &kGlobAccent);

    fontFace(NANOVG_DEJAVU_SANS_TTF);
    fontSize(kLabelFontSize);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    fillColor(toDglColor(kTheme.text));
    for (uint32_t i = 0; i < kNumParams; ++i)
    {
        if (bl::isFaderParam(i))
            continue;
        const bl::Rect& r = layout_.knob[i].label;
        text(r.x + r.w * 0.5f, r.y + r.h * 0.5f, kParams[i].label, nullptr);
    }

    master_.draw(*this);

    header_.drawTitle(*this, layout_.width); // base coordinates (UI auto-scale)
}

UI* createUI() { return new BastosUI(); }

END_NAMESPACE_DISTRHO
