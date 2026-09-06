#include "BastosPluginAdapter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

START_NAMESPACE_DISTRHO

namespace {
float dbToGain(float db) noexcept { return std::pow(10.f, db / 20.f); }
}

BastosPluginAdapter::BastosPluginAdapter()
    : Plugin(kParameterCount, 0, 0) // 13 parameters, 0 programs, 0 states -- meters are direct-access only
{
}

const char* BastosPluginAdapter::getLabel() const { return "Bastos"; }
const char* BastosPluginAdapter::getDescription() const { return "Bass enhancer and transient shaper"; }
const char* BastosPluginAdapter::getMaker() const { return "Spellbound"; }

const char* BastosPluginAdapter::getLicense() const
{
    // Must be a URI -- lv2lint's Plugin License test fails a plain word
    // like "Proprietary" (confirmed against Hex/Tank/Outflank).
    return "https://spellbound.audio/plugins/bastos#license";
}

uint32_t BastosPluginAdapter::getVersion() const { return d_version(0, 1, 0); }

void BastosPluginAdapter::initParameter(const uint32_t index, Parameter& parameter)
{
    switch (index)
    {
    case kParameterAtkGain:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Attack Gain"; parameter.symbol = "atk_gain"; parameter.unit = "dB";
        parameter.ranges.def = BASTOS_PARAM_ATK_GAIN_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_ATK_GAIN_MIN;
        parameter.ranges.max = BASTOS_PARAM_ATK_GAIN_MAX;
        break;
    case kParameterAtkSubCount:
        parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
        parameter.name = "Attack Sub Count"; parameter.symbol = "atk_sub_count";
        parameter.ranges.def = BASTOS_PARAM_ATK_SUB_COUNT_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_ATK_SUB_COUNT_MIN;
        parameter.ranges.max = BASTOS_PARAM_ATK_SUB_COUNT_MAX;
        break;
    case kParameterAtkSubLevel:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Attack Sub Level"; parameter.symbol = "atk_sub_level";
        parameter.ranges.def = BASTOS_PARAM_ATK_SUB_LEVEL_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_ATK_SUB_LEVEL_MIN;
        parameter.ranges.max = BASTOS_PARAM_ATK_SUB_LEVEL_MAX;
        break;
    case kParameterAtkUpperCount:
        parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
        parameter.name = "Attack Upper Count"; parameter.symbol = "atk_upper_count";
        parameter.ranges.def = BASTOS_PARAM_ATK_UPPER_COUNT_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_ATK_UPPER_COUNT_MIN;
        parameter.ranges.max = BASTOS_PARAM_ATK_UPPER_COUNT_MAX;
        break;
    case kParameterAtkUpperLevel:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Attack Upper Level"; parameter.symbol = "atk_upper_level";
        parameter.ranges.def = BASTOS_PARAM_ATK_UPPER_LEVEL_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_ATK_UPPER_LEVEL_MIN;
        parameter.ranges.max = BASTOS_PARAM_ATK_UPPER_LEVEL_MAX;
        break;
    case kParameterSusGain:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Sustain Gain"; parameter.symbol = "sus_gain"; parameter.unit = "dB";
        parameter.ranges.def = BASTOS_PARAM_SUS_GAIN_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SUS_GAIN_MIN;
        parameter.ranges.max = BASTOS_PARAM_SUS_GAIN_MAX;
        break;
    case kParameterSusSubCount:
        parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
        parameter.name = "Sustain Sub Count"; parameter.symbol = "sus_sub_count";
        parameter.ranges.def = BASTOS_PARAM_SUS_SUB_COUNT_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SUS_SUB_COUNT_MIN;
        parameter.ranges.max = BASTOS_PARAM_SUS_SUB_COUNT_MAX;
        break;
    case kParameterSusSubLevel:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Sustain Sub Level"; parameter.symbol = "sus_sub_level";
        parameter.ranges.def = BASTOS_PARAM_SUS_SUB_LEVEL_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SUS_SUB_LEVEL_MIN;
        parameter.ranges.max = BASTOS_PARAM_SUS_SUB_LEVEL_MAX;
        break;
    case kParameterSusUpperCount:
        parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
        parameter.name = "Sustain Upper Count"; parameter.symbol = "sus_upper_count";
        parameter.ranges.def = BASTOS_PARAM_SUS_UPPER_COUNT_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SUS_UPPER_COUNT_MIN;
        parameter.ranges.max = BASTOS_PARAM_SUS_UPPER_COUNT_MAX;
        break;
    case kParameterSusUpperLevel:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Sustain Upper Level"; parameter.symbol = "sus_upper_level";
        parameter.ranges.def = BASTOS_PARAM_SUS_UPPER_LEVEL_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SUS_UPPER_LEVEL_MIN;
        parameter.ranges.max = BASTOS_PARAM_SUS_UPPER_LEVEL_MAX;
        break;
    case kParameterSpeed:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Speed"; parameter.symbol = "speed";
        parameter.ranges.def = BASTOS_PARAM_SPEED_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_SPEED_MIN;
        parameter.ranges.max = BASTOS_PARAM_SPEED_MAX;
        break;
    case kParameterOutputGain:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Output Gain"; parameter.symbol = "output_gain"; parameter.unit = "dB";
        parameter.ranges.def = BASTOS_PARAM_OUTPUT_GAIN_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_OUTPUT_GAIN_MIN;
        parameter.ranges.max = BASTOS_PARAM_OUTPUT_GAIN_MAX;
        break;
    case kParameterMix:
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Mix"; parameter.symbol = "mix";
        parameter.ranges.def = BASTOS_PARAM_MIX_DEFAULT;
        parameter.ranges.min = BASTOS_PARAM_MIX_MIN;
        parameter.ranges.max = BASTOS_PARAM_MIX_MAX;
        break;
    default:
        break;
    }
}

float BastosPluginAdapter::getParameterValue(const uint32_t index) const
{
    switch (index)
    {
    case kParameterAtkGain:       return atkGainDb_;
    case kParameterAtkSubCount:   return atkSubCount_;
    case kParameterAtkSubLevel:   return atkSubLevel_;
    case kParameterAtkUpperCount: return atkUpperCount_;
    case kParameterAtkUpperLevel: return atkUpperLevel_;
    case kParameterSusGain:       return susGainDb_;
    case kParameterSusSubCount:   return susSubCount_;
    case kParameterSusSubLevel:   return susSubLevel_;
    case kParameterSusUpperCount: return susUpperCount_;
    case kParameterSusUpperLevel: return susUpperLevel_;
    case kParameterSpeed:         return speed_;
    case kParameterOutputGain:    return outputGainDb_;
    case kParameterMix:           return mix_;
    default:                      return 0.0f;
    }
}

void BastosPluginAdapter::setParameterValue(const uint32_t index, const float value)
{
    switch (index)
    {
    case kParameterAtkGain:       atkGainDb_ = value; break;
    case kParameterAtkSubCount:   atkSubCount_ = value; break;
    case kParameterAtkSubLevel:   atkSubLevel_ = value; break;
    case kParameterAtkUpperCount: atkUpperCount_ = value; break;
    case kParameterAtkUpperLevel: atkUpperLevel_ = value; break;
    case kParameterSusGain:       susGainDb_ = value; break;
    case kParameterSusSubCount:   susSubCount_ = value; break;
    case kParameterSusSubLevel:   susSubLevel_ = value; break;
    case kParameterSusUpperCount: susUpperCount_ = value; break;
    case kParameterSusUpperLevel: susUpperLevel_ = value; break;
    case kParameterSpeed:         speed_ = value; break;
    case kParameterOutputGain:    outputGainDb_ = value; break;
    case kParameterMix:           mix_ = value; break;
    default: break;
    }
}

void BastosPluginAdapter::activate()
{
    shaper_.prepare(getSampleRate());
    dryLeft_.assign(getBufferSize(), 0.f);
    dryRight_.assign(getBufferSize(), 0.f);
}

void BastosPluginAdapter::deactivate()
{
    shaper_.reset();
}

void BastosPluginAdapter::bufferSizeChanged(const uint32_t newBufferSize)
{
    dryLeft_.assign(newBufferSize, 0.f);
    dryRight_.assign(newBufferSize, 0.f);
}

void BastosPluginAdapter::run(const float** inputs, float** outputs, const uint32_t frames)
{
    // Input peak (matches the JUCE-era processBlock()'s per-block peak scan).
    float inPeak = 0.f;
    for (uint32_t i = 0; i < frames; ++i)
    {
        inPeak = std::max(inPeak, std::abs(inputs[0][i]));
        inPeak = std::max(inPeak, std::abs(inputs[1][i]));
    }
    inputLevelMeter_.write(inPeak);

    if (outputs[0] != inputs[0]) std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);
    if (outputs[1] != inputs[1]) std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);

    const bool wantsDryBlend = mix_ < 0.999f;
    if (wantsDryBlend)
    {
        std::memcpy(dryLeft_.data(),  outputs[0], sizeof(float) * frames);
        std::memcpy(dryRight_.data(), outputs[1], sizeof(float) * frames);
    }

    float* channels[2] = { outputs[0], outputs[1] };
    shaper_.process(channels, 2, static_cast<int>(frames),
                     atkGainDb_, static_cast<int>(atkSubCount_ + 0.5f), atkSubLevel_,
                     static_cast<int>(atkUpperCount_ + 0.5f), atkUpperLevel_,
                     susGainDb_, static_cast<int>(susSubCount_ + 0.5f), susSubLevel_,
                     static_cast<int>(susUpperCount_ + 0.5f), susUpperLevel_,
                     speed_, true);

    const float outGainLin = dbToGain(outputGainDb_);
    for (uint32_t i = 0; i < frames; ++i)
    {
        outputs[0][i] *= outGainLin;
        outputs[1][i] *= outGainLin;
    }

    if (wantsDryBlend)
    {
        const float dryAmount = 1.f - mix_;
        for (uint32_t i = 0; i < frames; ++i)
        {
            outputs[0][i] += dryAmount * dryLeft_[i];
            outputs[1][i] += dryAmount * dryRight_[i];
        }
    }

    float outPeak = 0.f;
    for (uint32_t i = 0; i < frames; ++i)
    {
        outPeak = std::max(outPeak, std::abs(outputs[0][i]));
        outPeak = std::max(outPeak, std::abs(outputs[1][i]));
    }
    outputLevelMeter_.write(outPeak);
}

Plugin* createPlugin() { return new BastosPluginAdapter(); }

END_NAMESPACE_DISTRHO
