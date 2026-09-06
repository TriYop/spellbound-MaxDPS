// Source/BastosPluginAdapter.h
#pragma once

#include "DistrhoPlugin.hpp"

START_NAMESPACE_DISTRHO

class BastosPluginAdapter : public Plugin
{
public:
    BastosPluginAdapter() : Plugin(kParameterCount, 0, 0) {}

protected:
    const char* getLabel() const override { return "Bastos"; }
    const char* getDescription() const override { return "Bass enhancer and transient shaper"; }
    const char* getMaker() const override { return "Spellbound"; }
    const char* getLicense() const override { return "https://spellbound.audio/plugins/bastos#license"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    void initParameter(uint32_t, Parameter&) override {}
    float getParameterValue(uint32_t) const override { return 0.0f; }
    void setParameterValue(uint32_t, float) override {}

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        // Stub: pass through untouched. Task 3 wires TransientShaperCore.
        if (outputs[0] != inputs[0]) std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);
        if (outputs[1] != inputs[1]) std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BastosPluginAdapter)
};

END_NAMESPACE_DISTRHO
