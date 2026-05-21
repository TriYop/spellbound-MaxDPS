#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// ── Parameter layout ──────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout BastosAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // ── Attack group ──────────────────────────────────────────────────────────
    params.push_back (std::make_unique<AudioParameterFloat> ("atk_gain", "Atk Gain",
        NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f,
        AudioParameterFloatAttributes{}.withLabel ("dB")));
    params.push_back (std::make_unique<AudioParameterInt>   ("atk_sub_count", "Atk Sub Count", 1, 3, 1));
    params.push_back (std::make_unique<AudioParameterFloat> ("atk_sub_level", "Atk Sub Level",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f));
    params.push_back (std::make_unique<AudioParameterInt>   ("atk_upper_count", "Atk Upper Count", 1, 5, 1));
    params.push_back (std::make_unique<AudioParameterFloat> ("atk_upper_level", "Atk Upper Level",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f));

    // ── Sustain group ─────────────────────────────────────────────────────────
    params.push_back (std::make_unique<AudioParameterFloat> ("sus_gain", "Sus Gain",
        NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f,
        AudioParameterFloatAttributes{}.withLabel ("dB")));
    params.push_back (std::make_unique<AudioParameterInt>   ("sus_sub_count", "Sus Sub Count", 1, 3, 1));
    params.push_back (std::make_unique<AudioParameterFloat> ("sus_sub_level", "Sus Sub Level",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f));
    params.push_back (std::make_unique<AudioParameterInt>   ("sus_upper_count", "Sus Upper Count", 1, 5, 1));
    params.push_back (std::make_unique<AudioParameterFloat> ("sus_upper_level", "Sus Upper Level",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f));

    // ── Global group ──────────────────────────────────────────────────────────
    params.push_back (std::make_unique<AudioParameterFloat> ("speed", "Speed",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f,
        AudioParameterFloatAttributes{}.withLabel ("")));
    params.push_back (std::make_unique<AudioParameterFloat> ("output_gain", "Output Gain",
        NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f,
        AudioParameterFloatAttributes{}.withLabel ("dB")));
    params.push_back (std::make_unique<AudioParameterFloat> ("mix", "Mix",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f,
        AudioParameterFloatAttributes{}.withLabel ("")));

    return { params.begin(), params.end() };
}

// ── Constructor ───────────────────────────────────────────────────────────────
BastosAudioProcessor::BastosAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Bastos", createParameterLayout())
{}

BastosAudioProcessor::~BastosAudioProcessor() = default;

// ── Info ──────────────────────────────────────────────────────────────────────
const juce::String BastosAudioProcessor::getName() const { return JucePlugin_Name; }
bool BastosAudioProcessor::acceptsMidi()  const { return false; }
bool BastosAudioProcessor::producesMidi() const { return false; }
bool BastosAudioProcessor::isMidiEffect() const { return false; }
double BastosAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  BastosAudioProcessor::getNumPrograms()              { return 1; }
int  BastosAudioProcessor::getCurrentProgram()           { return 0; }
void BastosAudioProcessor::setCurrentProgram (int)       {}
const juce::String BastosAudioProcessor::getProgramName (int) { return {}; }
void BastosAudioProcessor::changeProgramName (int, const juce::String&) {}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void BastosAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t> (samplesPerBlock);
    spec.numChannels      = 2;

    transientShaper_.prepare (spec);
}

void BastosAudioProcessor::releaseResources() {}

bool BastosAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    return true;
}

// ── processBlock ──────────────────────────────────────────────────────────────
void BastosAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nSamples  = buffer.getNumSamples();
    const int nChannels = buffer.getNumChannels();

    // Input peak
    float inPeak = 0.f;
    for (int ch = 0; ch < nChannels; ++ch)
        inPeak = std::max (inPeak, buffer.getMagnitude (ch, 0, nSamples));
    inputPeakDb.store (inPeak > 1e-7f ? juce::Decibels::gainToDecibels (inPeak) : -100.f,
                       std::memory_order_relaxed);

    // Read parameters once per block
    auto* avts = &apvts;
    auto load  = [avts] (const char* id) { return avts->getRawParameterValue (id)->load(); };

    const float atkGainDb     = load ("atk_gain");
    const int   atkSubCount   = static_cast<int> (load ("atk_sub_count")   + 0.5f);
    const float atkSubLevel   = load ("atk_sub_level");
    const int   atkUpperCount = static_cast<int> (load ("atk_upper_count") + 0.5f);
    const float atkUpperLevel = load ("atk_upper_level");

    const float susGainDb     = load ("sus_gain");
    const int   susSubCount   = static_cast<int> (load ("sus_sub_count")   + 0.5f);
    const float susSubLevel   = load ("sus_sub_level");
    const int   susUpperCount = static_cast<int> (load ("sus_upper_count") + 0.5f);
    const float susUpperLevel = load ("sus_upper_level");

    const float speed      = load ("speed");
    const float outputGain = load ("output_gain");
    const float mix        = load ("mix");

    // Dry/wet copy
    juce::AudioBuffer<float> dryBuf;
    if (mix < 0.999f)
    {
        dryBuf.setSize (nChannels, nSamples, false, false, true);
        for (int ch = 0; ch < nChannels; ++ch)
            dryBuf.copyFrom (ch, 0, buffer, ch, 0, nSamples);
    }

    transientShaper_.process (buffer,
                               atkGainDb, atkSubCount, atkSubLevel, atkUpperCount, atkUpperLevel,
                               susGainDb, susSubCount, susSubLevel, susUpperCount, susUpperLevel,
                               speed, true);

    buffer.applyGain (juce::Decibels::decibelsToGain (outputGain));

    if (mix < 0.999f)
        for (int ch = 0; ch < nChannels; ++ch)
            buffer.addFrom (ch, 0, dryBuf, ch, 0, nSamples, 1.f - mix);

    // Output peak
    float outPeak = 0.f;
    for (int ch = 0; ch < nChannels; ++ch)
        outPeak = std::max (outPeak, buffer.getMagnitude (ch, 0, nSamples));
    outputPeakDb.store (outPeak > 1e-7f ? juce::Decibels::gainToDecibels (outPeak) : -100.f,
                        std::memory_order_relaxed);
}

// ── State ─────────────────────────────────────────────────────────────────────
bool BastosAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* BastosAudioProcessor::createEditor()
{
    return new BastosAudioProcessorEditor (*this);
}

void BastosAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BastosAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BastosAudioProcessor();
}
