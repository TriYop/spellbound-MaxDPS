#pragma once

// Pure, framework-free geometry for Havoc' UI: header band on top, three
// ATTACK / SUSTAIN / GLOBAL group panels, and a full-height master strip
// (IN/OUT meters + Output Gain fader) on the right. No DGL/DPF includes so
// it is unit-testable; it consumes Common's framework-free layout helpers.

#include <audioplugins/common/hui/GroupLayout.h>
#include <audioplugins/common/hui/HeaderLayout.h>
#include <audioplugins/common/hui/MasterStripLayout.h>

#include <array>
#include <cstdint>

namespace havoc::layout {

namespace hui = audioplugins::common::hui;

struct Rect { float x = 0, y = 0, w = 0, h = 0; };
struct KnobSlot { Rect body; Rect label; };

// Parameter indices mirror the kParameter* enum in DistrhoPluginInfo.h (kept
// free of that header so this file stays pure geometry); HavocUI.cpp
// static_asserts the two stay in sync and the layout test checks them too.
constexpr uint32_t kAtkGain = 0, kAtkSubCount = 1, kAtkSubLevel = 2, kAtkUpperCount = 3,
                   kAtkUpperLevel = 4, kSusGain = 5, kSusSubCount = 6, kSusSubLevel = 7,
                   kSusUpperCount = 8, kSusUpperLevel = 9, kSpeed = 10, kOutputGain = 11,
                   kMix = 12;
constexpr uint32_t kParamCount = 13;

// Output Gain is the master strip fader, not a knob.
constexpr bool isFaderParam(uint32_t paramIndex) noexcept { return paramIndex == kOutputGain; }

struct UiLayout {
    float width = 0, height = 0;
    hui::HeaderLayout header;
    hui::GroupRect atk, sus, glob;
    std::array<KnobSlot, kParamCount> knob;  // by parameter index; knob[kOutputGain] unused (zero)
    hui::MasterRect master;
};

// The header shows the product hook under the title (PluginHeader::setTagline),
// which makes its band kHeaderTaglineH taller; content starts below that band.
constexpr bool kHeaderHasTagline = true;

constexpr float kMargin = 10, kGap = 8, kTopGap = 8;
constexpr float kKnobSize = 72, kLabelH = 14, kKnobGap = 8, kRowGap = 6;
constexpr float kRowPitch = kKnobSize + kLabelH + kRowGap;
constexpr float kRows = 3;
constexpr float kPairW = 2 * kKnobSize + kKnobGap;
constexpr float kSectionW = kPairW + 2 * hui::kGroupPad + 12;  // ATTACK / SUSTAIN
constexpr float kGlobalW = kKnobSize + 2 * hui::kGroupPad + 36;
constexpr float kGroupH = 2 * hui::kGroupPad + hui::kGroupTitleH + kRows * kRowPitch - kRowGap;

namespace detail {

// Knob centred in `content` at column `col` of `cols`, row `row`.
inline KnobSlot slot(const hui::GroupRect& content, int col, int cols, int row) noexcept
{
    const float used = static_cast<float>(cols) * kKnobSize + static_cast<float>(cols - 1) * kKnobGap;
    const float x = content.x + (content.w - used) / 2 + static_cast<float>(col) * (kKnobSize + kKnobGap);
    const float y = content.y + static_cast<float>(row) * kRowPitch;
    return {{x, y, kKnobSize, kKnobSize}, {x, y + kKnobSize, kKnobSize, kLabelH}};
}

} // namespace detail

inline UiLayout computeLayout() noexcept
{
    UiLayout l;
    l.width = 2 * kMargin + 2 * kSectionW + kGlobalW + hui::kMasterW + 3 * kGap;
    l.header = hui::computeHeaderLayout(l.width, kHeaderHasTagline);
    const float top = l.header.bandHeight + kTopGap;
    l.height = top + kGroupH + kMargin;

    l.atk = {kMargin, top, kSectionW, kGroupH};
    l.sus = {l.atk.x + kSectionW + kGap, top, kSectionW, kGroupH};
    l.glob = {l.sus.x + kSectionW + kGap, top, kGlobalW, kGroupH};
    l.master = {l.width - kMargin - hui::kMasterW, top, hui::kMasterW, kGroupH};

    const auto section = [&l](const hui::GroupRect& panel, uint32_t gain, uint32_t subCount,
                              uint32_t subLevel, uint32_t upperCount, uint32_t upperLevel) {
        const auto c = hui::computeGroupLayout(panel, true).content;
        l.knob[gain] = detail::slot(c, 0, 1, 0);
        l.knob[subCount] = detail::slot(c, 0, 2, 1);
        l.knob[subLevel] = detail::slot(c, 1, 2, 1);
        l.knob[upperCount] = detail::slot(c, 0, 2, 2);
        l.knob[upperLevel] = detail::slot(c, 1, 2, 2);
    };
    section(l.atk, kAtkGain, kAtkSubCount, kAtkSubLevel, kAtkUpperCount, kAtkUpperLevel);
    section(l.sus, kSusGain, kSusSubCount, kSusSubLevel, kSusUpperCount, kSusUpperLevel);

    const auto g = hui::computeGroupLayout(l.glob, true).content;
    l.knob[kSpeed] = detail::slot(g, 0, 1, 0);
    l.knob[kMix] = detail::slot(g, 0, 1, 1);
    return l;
}

} // namespace havoc::layout
