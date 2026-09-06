#include "BastosUI.h"
#include "FactoryPresets.h"

#include <cstdlib>
#include <string>

START_NAMESPACE_DISTRHO

namespace {

namespace hui = audioplugins::common::hui;

DGL_NAMESPACE::Color toDglColor(const hui::Colour& c) noexcept
{
    return DGL_NAMESPACE::Color(static_cast<int>(c.r), static_cast<int>(c.g),
                                 static_cast<int>(c.b), static_cast<float>(c.a) / 255.f);
}

// Palette ported from Source/_juce_reference/PluginEditor.cpp's kAtkAccent/
// kSusAccent/kGlobAccent (blue/teal/gold), reused as knob accent colors.
constexpr hui::Colour kBg        { 0x14, 0x14, 0x20, 0xff };
constexpr hui::Colour kSectionBg { 0x1e, 0x1e, 0x30, 0xff };
constexpr hui::Colour kAtkAccent { 0x44, 0xaa, 0xee, 0xff };
constexpr hui::Colour kSusAccent { 0x44, 0xdd, 0xaa, 0xff };
constexpr hui::Colour kGlobAccent{ 0xaa, 0xaa, 0x66, 0xff };

hui::dgl::RotaryKnobPalette paletteFor(const hui::Colour& accent)
{
    hui::dgl::RotaryKnobPalette p;
    p.valueArc = accent;
    p.valueArcGlow = accent;
    p.knobRim = accent;
    return p;
}

// Button/PresetSelector palettes: same section-bg/accent tones as the
// knobs, since both palette structs are deliberately generic (see Common's
// Button.h/PresetSelector.h) -- same convention as Hex's HexUI.cpp.
const hui::dgl::ButtonPalette kBastosButtonPalette = {
    /* background         */ kSectionBg,
    /* backgroundDisabled */ kBg,
    /* border             */ kGlobAccent,
    /* text               */ {0xdd, 0xdd, 0xdd, 0xff},
    /* textDisabled       */ {0x66, 0x66, 0x66, 0xff},
};

const hui::dgl::PresetSelectorPalette kBastosPresetSelectorPalette = {
    /* closedBackground */ kSectionBg,
    /* listBackground   */ kBg,
    /* border           */ kGlobAccent,
    /* text             */ {0xdd, 0xdd, 0xdd, 0xff},
    /* textFactory      */ {0xaa, 0xaa, 0xaa, 0xff},
    /* rowHighlight     */ {0xaa, 0xaa, 0x66, 0x40},
};

// Layout ported from the JUCE-era resized(): kW=660, kH=460, kKnobSize=72,
// three ATTACK/SUSTAIN/GLOBAL columns plus a 2-meter strip on the right.
// kH grows by kPresetBarH (24px) to fit the new preset bar above the
// existing panel; every Y origin below that used to start at kHeaderH +
// kMargin now starts kPresetBarH lower, matching Hex/Tank's precedent.
constexpr int kW = 660, kH = 484;
constexpr int kHeaderH = 36, kMargin = 10, kGap = 8;
constexpr int kKnobSize = 72, kLabelH = 16, kMeterW = 14;
constexpr int kAtkW = 240, kSusW = 240;

// Preset bar: a new 24px row above the header, left-aligned -- same
// constant values/spacing as Hex's HexUI.cpp (no reason to invent
// different numbers for the same-shaped UI element).
constexpr int kPresetBarH = 24;
constexpr float kPresetBarX = 8.0f;
constexpr float kPresetBarY = 8.0f;
constexpr uint  kPresetBarRowH = 24;
constexpr uint  kPresetSelectorW = 220;
constexpr uint  kPresetButtonW = 56;
constexpr float kPresetButtonGap = 6.0f;
constexpr float kSaveButtonX = kPresetBarX + static_cast<float>(kPresetSelectorW) + 8.0f;
constexpr float kDeleteButtonX = kSaveButtonX + static_cast<float>(kPresetButtonW) + kPresetButtonGap;

struct KnobSpec { float min, max, def; hui::Colour accent; int col, row; };

constexpr int kNumKnobSpecs = 13;

constexpr KnobSpec kKnobSpecs[kNumKnobSpecs] = {
    { BASTOS_PARAM_ATK_GAIN_MIN, BASTOS_PARAM_ATK_GAIN_MAX, BASTOS_PARAM_ATK_GAIN_DEFAULT, kAtkAccent, 0, 0 },
    { BASTOS_PARAM_ATK_SUB_COUNT_MIN, BASTOS_PARAM_ATK_SUB_COUNT_MAX, BASTOS_PARAM_ATK_SUB_COUNT_DEFAULT, kAtkAccent, 0, 1 },
    { BASTOS_PARAM_ATK_SUB_LEVEL_MIN, BASTOS_PARAM_ATK_SUB_LEVEL_MAX, BASTOS_PARAM_ATK_SUB_LEVEL_DEFAULT, kAtkAccent, 0, 1 },
    { BASTOS_PARAM_ATK_UPPER_COUNT_MIN, BASTOS_PARAM_ATK_UPPER_COUNT_MAX, BASTOS_PARAM_ATK_UPPER_COUNT_DEFAULT, kAtkAccent, 0, 2 },
    { BASTOS_PARAM_ATK_UPPER_LEVEL_MIN, BASTOS_PARAM_ATK_UPPER_LEVEL_MAX, BASTOS_PARAM_ATK_UPPER_LEVEL_DEFAULT, kAtkAccent, 0, 2 },
    { BASTOS_PARAM_SUS_GAIN_MIN, BASTOS_PARAM_SUS_GAIN_MAX, BASTOS_PARAM_SUS_GAIN_DEFAULT, kSusAccent, 1, 0 },
    { BASTOS_PARAM_SUS_SUB_COUNT_MIN, BASTOS_PARAM_SUS_SUB_COUNT_MAX, BASTOS_PARAM_SUS_SUB_COUNT_DEFAULT, kSusAccent, 1, 1 },
    { BASTOS_PARAM_SUS_SUB_LEVEL_MIN, BASTOS_PARAM_SUS_SUB_LEVEL_MAX, BASTOS_PARAM_SUS_SUB_LEVEL_DEFAULT, kSusAccent, 1, 1 },
    { BASTOS_PARAM_SUS_UPPER_COUNT_MIN, BASTOS_PARAM_SUS_UPPER_COUNT_MAX, BASTOS_PARAM_SUS_UPPER_COUNT_DEFAULT, kSusAccent, 1, 2 },
    { BASTOS_PARAM_SUS_UPPER_LEVEL_MIN, BASTOS_PARAM_SUS_UPPER_LEVEL_MAX, BASTOS_PARAM_SUS_UPPER_LEVEL_DEFAULT, kSusAccent, 1, 2 },
    { BASTOS_PARAM_SPEED_MIN, BASTOS_PARAM_SPEED_MAX, BASTOS_PARAM_SPEED_DEFAULT, kGlobAccent, 2, 0 },
    { BASTOS_PARAM_OUTPUT_GAIN_MIN, BASTOS_PARAM_OUTPUT_GAIN_MAX, BASTOS_PARAM_OUTPUT_GAIN_DEFAULT, kGlobAccent, 2, 1 },
    { BASTOS_PARAM_MIX_MIN, BASTOS_PARAM_MIX_MAX, BASTOS_PARAM_MIX_DEFAULT, kGlobAccent, 2, 2 },
};

// Helper shared by the constructor to avoid repeating the construct/size/
// position/palette/range/gesture-bracketing dance per knob. Gesture
// bracketing follows DPF's own idiom (examples/CairoUI: drag started/
// finished -> editParameter, value changed -> setParameterValue) -- same
// pattern as Hex's HexUI.cpp::makeKnob().
std::unique_ptr<hui::dgl::RotaryKnob> makeKnob(BastosUI& ui, const KnobSpec& spec,
                                                int x, int y, uint32_t paramIndex)
{
    std::unique_ptr<hui::dgl::RotaryKnob> knob(new hui::dgl::RotaryKnob(&ui));
    knob->setRange(spec.min, spec.max);
    knob->setDefaultValue(spec.def);
    knob->setValue(spec.def);
    knob->setPalette(paletteFor(spec.accent));
    knob->setAbsolutePos(x, y);
    knob->setSize(kKnobSize, kKnobSize);

    hui::dgl::RotaryKnob* const rawKnob = knob.get();
    rawKnob->onDragStateChanged = [&ui, paramIndex](const bool started)
    {
        ui.editParameter(paramIndex, started);
    };
    rawKnob->onValueChanged = [&ui, paramIndex](const float value)
    {
        ui.setParameterValue(paramIndex, value);
    };

    return knob;
}

std::unique_ptr<hui::dgl::PresetSelector> makePresetSelector(BastosUI& ui)
{
    std::unique_ptr<hui::dgl::PresetSelector> selector(new hui::dgl::PresetSelector(&ui));
    selector->setPalette(kBastosPresetSelectorPalette);
    selector->setClosedSize(kPresetSelectorW, kPresetBarRowH);
    selector->setAbsolutePos(static_cast<int>(kPresetBarX), static_cast<int>(kPresetBarY));
    return selector;
}

std::unique_ptr<hui::dgl::Button> makeButton(BastosUI& ui, const char* label, float x)
{
    std::unique_ptr<hui::dgl::Button> button(new hui::dgl::Button(&ui));
    button->setPalette(kBastosButtonPalette);
    button->setLabel(label);
    button->setSize(kPresetButtonW, kPresetBarRowH);
    button->setAbsolutePos(static_cast<int>(x), static_cast<int>(kPresetBarY));
    return button;
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

// Table-driven mapping between BASTOS_PARAM symbol strings and their DPF
// parameter index, shared by applyPreset() and captureCurrentParameters()
// so the 13 id strings are written exactly once, not duplicated in both
// directions.
struct ParamIdEntry { const char* id; uint32_t index; };

constexpr int kNumParamIds = 13;

constexpr ParamIdEntry kParamIds[kNumParamIds] = {
    {"atk_gain",        kParameterAtkGain},
    {"atk_sub_count",   kParameterAtkSubCount},
    {"atk_sub_level",   kParameterAtkSubLevel},
    {"atk_upper_count", kParameterAtkUpperCount},
    {"atk_upper_level", kParameterAtkUpperLevel},
    {"sus_gain",        kParameterSusGain},
    {"sus_sub_count",   kParameterSusSubCount},
    {"sus_sub_level",   kParameterSusSubLevel},
    {"sus_upper_count", kParameterSusUpperCount},
    {"sus_upper_level", kParameterSusUpperLevel},
    {"speed",           kParameterSpeed},
    {"output_gain",     kParameterOutputGain},
    {"mix",             kParameterMix},
};

} // namespace

BastosUI::BastosUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      pluginPtr_(static_cast<BastosPluginAdapter*>(getPluginInstancePointer())),
      presetBrowser_(bastosFactoryPresets(), bastosUserPresetsDirectory(), "com.spellbound.bastos"),
      presetSelector_(makePresetSelector(*this)),
      saveButton_(makeButton(*this, "SAVE", kSaveButtonX)),
      deleteButton_(makeButton(*this, "DELETE", kDeleteButtonX))
{
    const int colX[3] = { kMargin, kMargin + kAtkW + kGap, kMargin + kAtkW + kGap + kSusW + kGap };
    const int bodyY = kPresetBarH + kHeaderH + kMargin;
    const int rowH  = kKnobSize + kLabelH + kGap;

    // Two knobs per row for the sub/upper pairs (rows 1/2); one per row 0/global.
    int rowSlot[3][3] = {{0,0,0},{0,0,0},{0,0,0}};
    int slotOffsetInRow[kNumKnobs] = {0};
    for (int i = 0; i < kNumKnobs; ++i)
        slotOffsetInRow[i] = rowSlot[kKnobSpecs[i].col][kKnobSpecs[i].row]++;

    for (int i = 0; i < kNumKnobs; ++i)
    {
        const KnobSpec& spec = kKnobSpecs[i];
        const int x = colX[spec.col] + 4 + slotOffsetInRow[i] * (kKnobSize + 4);
        const int y = bodyY + 28 + spec.row * rowH;
        knobs_[static_cast<size_t>(i)] = makeKnob(*this, spec, x, y, static_cast<uint32_t>(i));
    }

    inMeter_  = std::make_unique<audioplugins::common::hui::dgl::VuMeter>(this);
    outMeter_ = std::make_unique<audioplugins::common::hui::dgl::VuMeter>(this);
    const int meterX = kW - kMargin - 2 * kMeterW - kGap;
    const int meterY = bodyY + 20;
    const int meterH = kH - meterY - kMargin - 10;
    inMeter_->setAbsolutePos(meterX, meterY);
    inMeter_->setSize(kMeterW, static_cast<uint>(meterH));
    outMeter_->setAbsolutePos(meterX + kMeterW + kGap, meterY);
    outMeter_->setSize(kMeterW, static_cast<uint>(meterH));

    presetSelector_->onIndexSelected = [this](const int index)
    {
        if (const auto* preset = presetBrowser_.selectIndex(index))
        {
            applyPreset(*preset);
            refreshPresetControls();
        }
    };

    deleteButton_->onClick = [this]()
    {
        if (presetBrowser_.deleteCurrent())
            refreshPresetControls();
    };

    saveButton_->onClick = [this]()
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

void BastosUI::applyPreset(const audioplugins::common::presets::Preset& preset)
{
    for (const auto& pv : preset.parameters)
    {
        for (const auto& entry : kParamIds)
        {
            if (pv.id == entry.id)
            {
                knobs_[entry.index]->setValue(pv.value);
                editParameter(entry.index, true);
                setParameterValue(entry.index, pv.value);
                editParameter(entry.index, false);
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
    parameters.reserve(kNumParamIds);
    for (const auto& entry : kParamIds)
        parameters.push_back({entry.id, knobs_[entry.index]->getValue()});
    return parameters;
}

void BastosUI::refreshPresetControls()
{
    presetSelector_->setEntries(presetBrowser_.getEntries());
    presetSelector_->setCurrentIndex(presetBrowser_.getCurrentIndex());

    const auto entries = presetBrowser_.getEntries();
    const int idx = presetBrowser_.getCurrentIndex();
    const bool isFactory = (idx >= 0 && static_cast<size_t>(idx) < entries.size())
                                ? entries[static_cast<size_t>(idx)].isFactory
                                : true;
    deleteButton_->setEnabled(!isFactory);
}

void BastosUI::parameterChanged(const uint32_t index, const float value)
{
    setKnobValue(index, value);
}

void BastosUI::setKnobValue(const uint32_t index, const float value)
{
    if (index < static_cast<uint32_t>(kNumKnobs))
        knobs_[index]->setValue(value);
}

void BastosUI::uiIdle()
{
    // Direct-access read of the DSP instance's atomically-published peak
    // meters -- see DistrhoPluginInfo.h's DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
    // comment for why this isn't a DPF parameter. pluginPtr_ is confirmed
    // non-null for every target Bastos currently builds (DPF passes the
    // real DSP instance pointer to every UI it constructs), but this derefs
    // it every idle tick, so guard it anyway as cheap insurance.
    if (pluginPtr_ == nullptr)
        return;

    inMeter_->pushLevel(pluginPtr_->getInputLevel());
    outMeter_->pushLevel(pluginPtr_->getOutputLevel());
}

void BastosUI::onNanoDisplay()
{
    beginPath();
    rect(0, 0, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillColor(toDglColor(kBg));
    fill();
    closePath();

    // Section backgrounds -- plain rounded rects, ported from the JUCE-era
    // drawSectionBg() (no reusable "panel" widget exists in Common, and one
    // rounded rect + a label doesn't warrant adding one).
    const int bodyY = kPresetBarH + kHeaderH + kMargin;
    const int bodyH = kH - bodyY - kMargin;
    const int atkX = kMargin, susX = atkX + kAtkW + kGap, globX = susX + kSusW + kGap;
    const int globW = kW - globX - kMargin - 2 * kMeterW - kGap * 2;

    auto drawPanel = [this](int x, int y, int w, int h) {
        beginPath();
        roundedRect(static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(w), static_cast<float>(h), 6.f);
        fillColor(toDglColor(kSectionBg));
        fill();
        closePath();
    };
    drawPanel(atkX, bodyY, kAtkW, bodyH);
    drawPanel(susX, bodyY, kSusW, bodyH);
    drawPanel(globX, bodyY, globW, bodyH);
}

UI* createUI() { return new BastosUI(); }

END_NAMESPACE_DISTRHO
