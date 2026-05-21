#include "PluginEditor.h"
#include <cmath>

// ── Palette ───────────────────────────────────────────────────────────────────
static const juce::Colour kBg         { 0xff141420 };
static const juce::Colour kSectionBg  { 0xff1e1e30 };
static const juce::Colour kLabel      { 0xffaaaacc };
static const juce::Colour kDim        { 0xff666688 };
static const juce::Colour kAtkAccent  { 0xff44aaee };   // blue — attack
static const juce::Colour kSusAccent  { 0xff44ddaa };   // teal — sustain
static const juce::Colour kGlobAccent { 0xffaaaa66 };   // gold — global
static const juce::Colour kMeterGood  { 0xff44cc88 };
static const juce::Colour kMeterWarn  { 0xffffaa00 };
static const juce::Colour kMeterClip  { 0xffee4444 };

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr int kW         = 660;
static constexpr int kH         = 460;
static constexpr int kHeaderH   = 36;
static constexpr int kMargin    = 10;
static constexpr int kGap       = 8;
static constexpr int kMeterW    = 14;
static constexpr int kKnobSize  = 72;
static constexpr int kLabelH    = 16;
static constexpr int kSectionTitleH = 18;

// ── Helpers ───────────────────────────────────────────────────────────────────
void BastosAudioProcessorEditor::addKnob (juce::Slider& s, juce::Label& l,
                                           const char* name, juce::Colour accent)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 14);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff333355));
    s.setColour (juce::Slider::thumbColourId,              accent.brighter (0.3f));
    s.setColour (juce::Slider::textBoxTextColourId,        kLabel);
    s.setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setFont (juce::FontOptions (10.f));
    l.setColour (juce::Label::textColourId, kDim);
    l.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (l);
}

void BastosAudioProcessorEditor::addIntKnob (juce::Slider& s, juce::Label& l,
                                              const char* name, juce::Colour accent)
{
    addKnob (s, l, name, accent);
    s.setNumDecimalPlacesToDisplay (0);
}

void BastosAudioProcessorEditor::drawSectionBg (juce::Graphics& g,
                                                 juce::Rectangle<int> area,
                                                 const char* title) const
{
    g.setColour (kSectionBg);
    g.fillRoundedRectangle (area.toFloat(), 6.f);

    g.setColour (kDim);
    g.setFont (juce::FontOptions (11.f).withStyle ("Bold"));
    g.drawText (title, area.getX() + 8, area.getY() + 4, area.getWidth() - 16, kSectionTitleH,
                juce::Justification::left);
}

void BastosAudioProcessorEditor::drawMeter (juce::Graphics& g,
                                             juce::Rectangle<int> area,
                                             float peakDb,
                                             const char* label) const
{
    g.setColour (juce::Colour (0xff0d0d1a));
    g.fillRoundedRectangle (area.toFloat(), 3.f);

    const float norm  = std::clamp ((peakDb + 60.f) / 60.f, 0.f, 1.f);
    const int   fillH = static_cast<int> (area.getHeight() * norm);
    if (fillH > 0)
    {
        auto fill = area.removeFromBottom (fillH);
        const auto col = peakDb >= 0.f  ? kMeterClip :
                         peakDb >= -6.f ? kMeterWarn : kMeterGood;
        g.setColour (col);
        g.fillRoundedRectangle (fill.toFloat(), 2.f);
    }

    g.setColour (kDim);
    g.setFont (juce::FontOptions (9.f));
    g.drawText (label, area.getX() - 2, area.getY() - 14, area.getWidth() + 4, 12,
                juce::Justification::centred);
}

// ── Constructor ───────────────────────────────────────────────────────────────
BastosAudioProcessorEditor::BastosAudioProcessorEditor (BastosAudioProcessor& p)
    : AudioProcessorEditor (&p), proc_ (p)
{
    auto& apvts = proc_.apvts;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    // ── Attack knobs ──────────────────────────────────────────────────────────
    addKnob    (atkGainKnob_,       atkGainLbl_,       "Gain",    kAtkAccent);
    addIntKnob (atkSubCountKnob_,   atkSubCountLbl_,   "Sub #",   kAtkAccent);
    addKnob    (atkSubLevelKnob_,   atkSubLevelLbl_,   "Sub Lvl", kAtkAccent);
    addIntKnob (atkUpperCountKnob_, atkUpperCountLbl_, "Upper #", kAtkAccent);
    addKnob    (atkUpperLevelKnob_, atkUpperLevelLbl_, "Up Lvl",  kAtkAccent);

    atkGainAtt_       = std::make_unique<SA> (apvts, "atk_gain",        atkGainKnob_);
    atkSubCountAtt_   = std::make_unique<SA> (apvts, "atk_sub_count",   atkSubCountKnob_);
    atkSubLevelAtt_   = std::make_unique<SA> (apvts, "atk_sub_level",   atkSubLevelKnob_);
    atkUpperCountAtt_ = std::make_unique<SA> (apvts, "atk_upper_count", atkUpperCountKnob_);
    atkUpperLevelAtt_ = std::make_unique<SA> (apvts, "atk_upper_level", atkUpperLevelKnob_);

    // ── Sustain knobs ─────────────────────────────────────────────────────────
    addKnob    (susGainKnob_,       susGainLbl_,       "Gain",    kSusAccent);
    addIntKnob (susSubCountKnob_,   susSubCountLbl_,   "Sub #",   kSusAccent);
    addKnob    (susSubLevelKnob_,   susSubLevelLbl_,   "Sub Lvl", kSusAccent);
    addIntKnob (susUpperCountKnob_, susUpperCountLbl_, "Upper #", kSusAccent);
    addKnob    (susUpperLevelKnob_, susUpperLevelLbl_, "Up Lvl",  kSusAccent);

    susGainAtt_       = std::make_unique<SA> (apvts, "sus_gain",        susGainKnob_);
    susSubCountAtt_   = std::make_unique<SA> (apvts, "sus_sub_count",   susSubCountKnob_);
    susSubLevelAtt_   = std::make_unique<SA> (apvts, "sus_sub_level",   susSubLevelKnob_);
    susUpperCountAtt_ = std::make_unique<SA> (apvts, "sus_upper_count", susUpperCountKnob_);
    susUpperLevelAtt_ = std::make_unique<SA> (apvts, "sus_upper_level", susUpperLevelKnob_);

    // ── Global knobs ──────────────────────────────────────────────────────────
    addKnob (speedKnob_,      speedLbl_,      "Speed", kGlobAccent);
    addKnob (outputGainKnob_, outputGainLbl_, "Out",   kGlobAccent);
    addKnob (mixKnob_,        mixLbl_,        "Mix",   kGlobAccent);

    speedAtt_      = std::make_unique<SA> (apvts, "speed",       speedKnob_);
    outputGainAtt_ = std::make_unique<SA> (apvts, "output_gain", outputGainKnob_);
    mixAtt_        = std::make_unique<SA> (apvts, "mix",         mixKnob_);

    setSize (kW, kH);
    startTimerHz (30);
}

BastosAudioProcessorEditor::~BastosAudioProcessorEditor()
{
    stopTimer();
}

// ── Timer ─────────────────────────────────────────────────────────────────────
void BastosAudioProcessorEditor::timerCallback()
{
    displayInputPeak_  = proc_.inputPeakDb .load (std::memory_order_relaxed);
    displayOutputPeak_ = proc_.outputPeakDb.load (std::memory_order_relaxed);
    repaint();
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void BastosAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Header
    g.setColour (kLabel);
    g.setFont (juce::FontOptions (18.f).withStyle ("Bold"));
    g.drawText ("BASTOS", kMargin, 8, 120, kHeaderH - 8, juce::Justification::left);

    // Section backgrounds (drawn before knobs so knobs appear on top)
    const int bodyY  = kHeaderH + kMargin;
    const int bodyH  = kH - bodyY - kMargin;
    const int atkW   = 240;
    const int susX   = kMargin + atkW + kGap;
    const int susW   = 240;
    const int globX  = susX + susW + kGap;
    const int globW  = kW - globX - kMargin - 2 * kMeterW - kGap * 2;

    drawSectionBg (g, { kMargin, bodyY, atkW,  bodyH }, "ATTACK");
    drawSectionBg (g, { susX,    bodyY, susW,  bodyH }, "SUSTAIN");
    drawSectionBg (g, { globX,   bodyY, globW, bodyH }, "GLOBAL");

    // Level meters
    const int meterX = kW - kMargin - 2 * kMeterW - kGap;
    const int meterY = bodyY + 20;
    const int meterH = bodyH - 30;
    drawMeter (g, { meterX,              meterY, kMeterW, meterH }, displayInputPeak_,  "IN");
    drawMeter (g, { meterX + kMeterW + kGap, meterY, kMeterW, meterH }, displayOutputPeak_, "OUT");
}

// ── Resized ───────────────────────────────────────────────────────────────────
void BastosAudioProcessorEditor::resized()
{
    const int bodyY   = kHeaderH + kMargin;
    const int atkX    = kMargin;
    const int atkW    = 240;
    const int susX    = atkX + atkW + kGap;
    const int susW    = 240;
    const int globX   = susX + susW + kGap;

    // Helper: place a row of knobs starting at (x, y) with given width per slot
    const int slotW  = kKnobSize + 4;
    const int rowH   = kKnobSize + kLabelH;

    auto placeKnob = [&] (juce::Slider& s, juce::Label& l, int x, int y)
    {
        s.setBounds (x, y, kKnobSize, kKnobSize);
        l.setBounds (x, y + kKnobSize, kKnobSize, kLabelH);
    };

    // Three content rows per section: Gain / Sub harmonics / Upper harmonics
    const int row1Y = bodyY + kSectionTitleH + 10;
    const int row2Y = row1Y + rowH + kGap;   // sub row
    const int row3Y = row2Y + rowH + kGap;   // upper row

    // Attack group
    placeKnob (atkGainKnob_,       atkGainLbl_,       atkX + 4,         row1Y);
    placeKnob (atkSubCountKnob_,   atkSubCountLbl_,   atkX + 4,         row2Y);
    placeKnob (atkSubLevelKnob_,   atkSubLevelLbl_,   atkX + 4 + slotW, row2Y);
    placeKnob (atkUpperCountKnob_, atkUpperCountLbl_, atkX + 4,         row3Y);
    placeKnob (atkUpperLevelKnob_, atkUpperLevelLbl_, atkX + 4 + slotW, row3Y);

    // Sustain group
    placeKnob (susGainKnob_,       susGainLbl_,       susX + 4,         row1Y);
    placeKnob (susSubCountKnob_,   susSubCountLbl_,   susX + 4,         row2Y);
    placeKnob (susSubLevelKnob_,   susSubLevelLbl_,   susX + 4 + slotW, row2Y);
    placeKnob (susUpperCountKnob_, susUpperCountLbl_, susX + 4,         row3Y);
    placeKnob (susUpperLevelKnob_, susUpperLevelLbl_, susX + 4 + slotW, row3Y);

    // Global group: single column
    const int gRow1 = row1Y;
    const int gRow2 = gRow1 + rowH + kGap;
    const int gRow3 = gRow2 + rowH + kGap;
    placeKnob (speedKnob_,      speedLbl_,      globX + 4, gRow1);
    placeKnob (outputGainKnob_, outputGainLbl_, globX + 4, gRow2);
    placeKnob (mixKnob_,        mixLbl_,        globX + 4, gRow3);
}
