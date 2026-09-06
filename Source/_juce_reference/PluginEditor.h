#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class BastosAudioProcessorEditor final
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit BastosAudioProcessorEditor (BastosAudioProcessor&);
    ~BastosAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void drawMeter (juce::Graphics&, juce::Rectangle<int> area,
                    float peakDb, const char* label) const;

    void drawSectionBg (juce::Graphics&, juce::Rectangle<int> area,
                        const char* title) const;

    // Helpers to build a rotary knob + label pair
    void addKnob (juce::Slider& s, juce::Label& l, const char* name, juce::Colour accent);
    void addIntKnob (juce::Slider& s, juce::Label& l, const char* name, juce::Colour accent);

    BastosAudioProcessor& proc_;

    // ── Attack group ──────────────────────────────────────────────────────────
    juce::Slider atkGainKnob_, atkSubCountKnob_, atkSubLevelKnob_,
                 atkUpperCountKnob_, atkUpperLevelKnob_;
    juce::Label  atkGainLbl_, atkSubCountLbl_, atkSubLevelLbl_,
                 atkUpperCountLbl_, atkUpperLevelLbl_;

    // ── Sustain group ─────────────────────────────────────────────────────────
    juce::Slider susGainKnob_, susSubCountKnob_, susSubLevelKnob_,
                 susUpperCountKnob_, susUpperLevelKnob_;
    juce::Label  susGainLbl_, susSubCountLbl_, susSubLevelLbl_,
                 susUpperCountLbl_, susUpperLevelLbl_;

    // ── Global group ──────────────────────────────────────────────────────────
    juce::Slider speedKnob_, outputGainKnob_, mixKnob_;
    juce::Label  speedLbl_,  outputGainLbl_,  mixLbl_;

    float displayInputPeak_  { -100.f };
    float displayOutputPeak_ { -100.f };

    // APVTS attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<SliderAttachment>
        atkGainAtt_, atkSubCountAtt_, atkSubLevelAtt_, atkUpperCountAtt_, atkUpperLevelAtt_,
        susGainAtt_, susSubCountAtt_, susSubLevelAtt_, susUpperCountAtt_, susUpperLevelAtt_,
        speedAtt_, outputGainAtt_, mixAtt_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BastosAudioProcessorEditor)
};
