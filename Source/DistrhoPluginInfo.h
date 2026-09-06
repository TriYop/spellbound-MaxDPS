/*
 * Spellbound Bastos — DPF plugin metadata.
 *
 * Bastos is a stereo effect, not a synth: it does not want MIDI input, and
 * it processes two audio inputs into two audio outputs. It reports zero
 * latency -- TransientShaperCore (Source/DSP/TransientShaperCore.h) has no
 * delay lines, only per-sample IIR/envelope math.
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Spellbound"
#define DISTRHO_PLUGIN_NAME    "Bastos"
#define DISTRHO_PLUGIN_URI     "https://spellbound.audio/plugins/bastos"
#define DISTRHO_PLUGIN_CLAP_ID "com.spellbound.bastos"

#define DISTRHO_PLUGIN_BRAND_ID  Spbd
#define DISTRHO_PLUGIN_UNIQUE_ID Bsts

#define DISTRHO_PLUGIN_HAS_UI      1
#define DISTRHO_PLUGIN_IS_RT_SAFE  1
#define DISTRHO_PLUGIN_IS_SYNTH    0
#define DISTRHO_PLUGIN_NUM_INPUTS  2
#define DISTRHO_PLUGIN_NUM_OUTPUTS 2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 0
#define DISTRHO_PLUGIN_WANT_LATENCY    0

/*
 * Input/Output peak meters are never declared as a host parameter (or DPF
 * State) on any format -- clap-validator fails any host-visible, audio-
 * reactive output parameter regardless of hints (see Hex's CLAUDE.md's
 * param-set-events/param-set-no-cookies fix). Instead BastosUI reads
 * BastosPluginAdapter directly every idle tick via
 * DISTRHO_PLUGIN_WANT_DIRECT_ACCESS + UI::getPluginInstancePointer() -- same
 * idiom Hex/Tank use.
 */
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1
// Forced to 0 for the same reason documented in every prior migration's
// DistrhoPluginInfo.h: DPF defaults this to DISTRHO_PLUGIN_WANT_DIRECT_ACCESS's
// value when unset, which is wrong for a CMake build producing two separate
// LV2 modules (Bastos_dsp.so, Bastos_ui.so), not one combined object.
#define DISTRHO_PLUGIN_AND_UI_IN_SINGLE_OBJECT 0

#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Dynamics"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "audio-effect", "utility", "transient-shaper"

#define DISTRHO_UI_USE_NANOVG     1
#define DISTRHO_UI_USER_RESIZABLE 0
#define DISTRHO_UI_DEFAULT_WIDTH  660
#define DISTRHO_UI_DEFAULT_HEIGHT 460
#define DISTRHO_UI_FILE_BROWSER   1

/*
 * Host parameter indices, shared between BastosPluginAdapter and BastosUI.
 * Declared here (not in BastosPluginAdapter.h) so BastosUI.cpp can use them
 * without pulling in DistrhoPlugin.hpp — same placement Hex/Tank/Outflank use.
 *
 * NOTE: this is the REAL, shipped parameter set (verified by reading
 * Source/_juce_reference/PluginProcessor.cpp::createParameterLayout()
 * directly) -- NOT the fictional bass_enabled/drive/cutoff/blend/
 * transient_enabled/bp_* parameters the pre-migration CLAUDE.md described.
 * There is no bypass parameter of any kind.
 */
enum BastosParameters {
    kParameterAtkGain = 0,
    kParameterAtkSubCount,
    kParameterAtkSubLevel,
    kParameterAtkUpperCount,
    kParameterAtkUpperLevel,
    kParameterSusGain,
    kParameterSusSubCount,
    kParameterSusSubLevel,
    kParameterSusUpperCount,
    kParameterSusUpperLevel,
    kParameterSpeed,
    kParameterOutputGain,
    kParameterMix,
    kParameterCount   // 13
};

/* Ranges/defaults carried over verbatim from the JUCE-era
   Source/_juce_reference/PluginProcessor.cpp::createParameterLayout(). */
#define BASTOS_PARAM_ATK_GAIN_MIN      -12.0f
#define BASTOS_PARAM_ATK_GAIN_MAX      12.0f
#define BASTOS_PARAM_ATK_GAIN_DEFAULT  0.0f

#define BASTOS_PARAM_ATK_SUB_COUNT_MIN     1.0f
#define BASTOS_PARAM_ATK_SUB_COUNT_MAX     3.0f
#define BASTOS_PARAM_ATK_SUB_COUNT_DEFAULT 1.0f

#define BASTOS_PARAM_ATK_SUB_LEVEL_MIN     0.0f
#define BASTOS_PARAM_ATK_SUB_LEVEL_MAX     1.0f
#define BASTOS_PARAM_ATK_SUB_LEVEL_DEFAULT 0.0f

#define BASTOS_PARAM_ATK_UPPER_COUNT_MIN     1.0f
#define BASTOS_PARAM_ATK_UPPER_COUNT_MAX     5.0f
#define BASTOS_PARAM_ATK_UPPER_COUNT_DEFAULT 1.0f

#define BASTOS_PARAM_ATK_UPPER_LEVEL_MIN     0.0f
#define BASTOS_PARAM_ATK_UPPER_LEVEL_MAX     1.0f
#define BASTOS_PARAM_ATK_UPPER_LEVEL_DEFAULT 0.0f

#define BASTOS_PARAM_SUS_GAIN_MIN      -12.0f
#define BASTOS_PARAM_SUS_GAIN_MAX      12.0f
#define BASTOS_PARAM_SUS_GAIN_DEFAULT  0.0f

#define BASTOS_PARAM_SUS_SUB_COUNT_MIN     1.0f
#define BASTOS_PARAM_SUS_SUB_COUNT_MAX     3.0f
#define BASTOS_PARAM_SUS_SUB_COUNT_DEFAULT 1.0f

#define BASTOS_PARAM_SUS_SUB_LEVEL_MIN     0.0f
#define BASTOS_PARAM_SUS_SUB_LEVEL_MAX     1.0f
#define BASTOS_PARAM_SUS_SUB_LEVEL_DEFAULT 0.0f

#define BASTOS_PARAM_SUS_UPPER_COUNT_MIN     1.0f
#define BASTOS_PARAM_SUS_UPPER_COUNT_MAX     5.0f
#define BASTOS_PARAM_SUS_UPPER_COUNT_DEFAULT 1.0f

#define BASTOS_PARAM_SUS_UPPER_LEVEL_MIN     0.0f
#define BASTOS_PARAM_SUS_UPPER_LEVEL_MAX     1.0f
#define BASTOS_PARAM_SUS_UPPER_LEVEL_DEFAULT 0.0f

#define BASTOS_PARAM_SPEED_MIN     0.0f
#define BASTOS_PARAM_SPEED_MAX     1.0f
#define BASTOS_PARAM_SPEED_DEFAULT 0.0f

#define BASTOS_PARAM_OUTPUT_GAIN_MIN      -12.0f
#define BASTOS_PARAM_OUTPUT_GAIN_MAX      12.0f
#define BASTOS_PARAM_OUTPUT_GAIN_DEFAULT  0.0f

#define BASTOS_PARAM_MIX_MIN     0.0f
#define BASTOS_PARAM_MIX_MAX     1.0f
#define BASTOS_PARAM_MIX_DEFAULT 1.0f

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
