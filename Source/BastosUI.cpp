#include "BastosUI.h"

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

// Layout ported from the JUCE-era resized(): kW=660, kH=460, kKnobSize=72,
// three ATTACK/SUSTAIN/GLOBAL columns plus a 2-meter strip on the right.
constexpr int kW = 660, kH = 460;
constexpr int kHeaderH = 36, kMargin = 10, kGap = 8;
constexpr int kKnobSize = 72, kLabelH = 16, kMeterW = 14;
constexpr int kAtkW = 240, kSusW = 240;

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

} // namespace

BastosUI::BastosUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      pluginPtr_(static_cast<BastosPluginAdapter*>(getPluginInstancePointer()))
{
    const int colX[3] = { kMargin, kMargin + kAtkW + kGap, kMargin + kAtkW + kGap + kSusW + kGap };
    const int bodyY = kHeaderH + kMargin;
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
    const int bodyY = kHeaderH + kMargin;
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
