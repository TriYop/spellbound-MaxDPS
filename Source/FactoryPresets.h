#pragma once

#include "audioplugins/common/presets/Preset.h"

#include <vector>

// Bastos's factory presets -- authored from scratch for this migration
// (the JUCE-era plugin never had a presets system to convert, unlike Hex's
// hand-converted XML-derived presets). Parameter ids match Source/
// DistrhoPluginInfo.h's symbol strings exactly (atk_gain, atk_sub_count, ...).
inline std::vector<audioplugins::common::presets::Preset> bastosFactoryPresets()
{
    using audioplugins::common::presets::Preset;
    std::vector<Preset> presets;

    {
        // Punchy kick: strong attack transient boost with sub-harmonic
        // reinforcement, minimal sustain shaping.
        Preset p;
        p.name = "Punchy Kick";
        p.pluginId = "com.spellbound.bastos";
        p.schemaVersion = 1;
        p.parameters = {
            {"atk_gain", 8.0f}, {"atk_sub_count", 2.0f}, {"atk_sub_level", 0.6f},
            {"atk_upper_count", 2.0f}, {"atk_upper_level", 0.3f},
            {"sus_gain", -2.0f}, {"sus_sub_count", 1.0f}, {"sus_sub_level", 0.1f},
            {"sus_upper_count", 1.0f}, {"sus_upper_level", 0.0f},
            {"speed", 0.2f}, {"output_gain", 0.0f}, {"mix", 1.0f},
        };
        presets.push_back(std::move(p));
    }
    {
        // Sub reinforcement: heavy sub-harmonic generation on both attack
        // and sustain, gentle gain shaping -- thickens thin kick samples.
        Preset p;
        p.name = "Sub Reinforce";
        p.pluginId = "com.spellbound.bastos";
        p.schemaVersion = 1;
        p.parameters = {
            {"atk_gain", 2.0f}, {"atk_sub_count", 3.0f}, {"atk_sub_level", 0.8f},
            {"atk_upper_count", 1.0f}, {"atk_upper_level", 0.0f},
            {"sus_gain", 1.0f}, {"sus_sub_count", 3.0f}, {"sus_sub_level", 0.7f},
            {"sus_upper_count", 1.0f}, {"sus_upper_level", 0.0f},
            {"speed", 0.5f}, {"output_gain", -1.0f}, {"mix", 1.0f},
        };
        presets.push_back(std::move(p));
    }
    {
        // Snappy transient: fast envelope (low speed), strong upper-harmonic
        // click on attack, sustain pulled down for a tighter, snappier feel.
        Preset p;
        p.name = "Snappy Transient";
        p.pluginId = "com.spellbound.bastos";
        p.schemaVersion = 1;
        p.parameters = {
            {"atk_gain", 10.0f}, {"atk_sub_count", 1.0f}, {"atk_sub_level", 0.2f},
            {"atk_upper_count", 4.0f}, {"atk_upper_level", 0.6f},
            {"sus_gain", -8.0f}, {"sus_sub_count", 1.0f}, {"sus_sub_level", 0.0f},
            {"sus_upper_count", 1.0f}, {"sus_upper_level", 0.0f},
            {"speed", 0.0f}, {"output_gain", -2.0f}, {"mix", 1.0f},
        };
        presets.push_back(std::move(p));
    }

    return presets;
}
