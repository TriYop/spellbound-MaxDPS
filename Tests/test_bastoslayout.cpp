#include "test_runner.h"
#include "../Source/BastosLayout.h"

#include <audioplugins/common/hui/GroupLayout.h>
#include <audioplugins/common/hui/HeaderLayout.h>
#include <audioplugins/common/hui/MasterStripLayout.h>

#include <array>
#include <cstdint>

namespace hui = audioplugins::common::hui;
namespace bl = bastos::layout;

namespace {

struct R { float x, y, w, h; };

R r(const bl::Rect& v) { return {v.x, v.y, v.w, v.h}; }
R r(const hui::GroupRect& v) { return {v.x, v.y, v.w, v.h}; }
R r(const hui::MasterRect& v) { return {v.x, v.y, v.w, v.h}; }

bool inside(const R& in, const R& out)
{
    return in.x >= out.x && in.y >= out.y && in.x + in.w <= out.x + out.w &&
           in.y + in.h <= out.y + out.h;
}

bool overlap(const R& a, const R& b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

struct Group { const char* name; hui::GroupRect panel; std::array<uint32_t, 5> params; int count; };

std::array<Group, 3> groups(const bl::UiLayout& l)
{
    return {{
        {"atk", l.atk, {bl::kAtkGain, bl::kAtkSubCount, bl::kAtkSubLevel, bl::kAtkUpperCount, bl::kAtkUpperLevel}, 5},
        {"sus", l.sus, {bl::kSusGain, bl::kSusSubCount, bl::kSusSubLevel, bl::kSusUpperCount, bl::kSusUpperLevel}, 5},
        {"glob", l.glob, {bl::kSpeed, bl::kMix, 0, 0, 0}, 2},
    }};
}

void all_knobs_inside_their_group()
{
    const auto l = bl::computeLayout();
    for (const auto& g : groups(l))
    {
        const R content = r(hui::computeGroupLayout(g.panel, true).content);
        for (int i = 0; i < g.count; ++i)
        {
            const auto& k = l.knob[g.params[static_cast<size_t>(i)]];
            CHECK_MSG(inside(r(k.body), content), g.name);
            CHECK_MSG(inside(r(k.label), content), g.name);
            CHECK_MSG(!overlap(r(k.body), r(k.label)), g.name);
            CHECK_MSG(k.label.w > 0 && k.label.h > 0, g.name);
        }
    }
}

void no_overlap()
{
    const auto l = bl::computeLayout();
    std::array<R, 12> bodies{};
    std::array<R, 12> labels{};
    int n = 0;
    for (const auto& g : groups(l))
        for (int i = 0; i < g.count; ++i)
        {
            bodies[static_cast<size_t>(n)] = r(l.knob[g.params[static_cast<size_t>(i)]].body);
            labels[static_cast<size_t>(n)] = r(l.knob[g.params[static_cast<size_t>(i)]].label);
            ++n;
        }
    CHECK(n == 12);
    for (int a = 0; a < n; ++a)
        for (int b = 0; b < n; ++b)
        {
            if (a != b)
            {
                CHECK(!overlap(bodies[static_cast<size_t>(a)], bodies[static_cast<size_t>(b)]));
                CHECK(!overlap(labels[static_cast<size_t>(a)], labels[static_cast<size_t>(b)]));
                CHECK(!overlap(bodies[static_cast<size_t>(a)], labels[static_cast<size_t>(b)]));
            }
        }

    const std::array<R, 4> blocks = {r(l.atk), r(l.sus), r(l.glob), r(l.master)};
    for (size_t a = 0; a < blocks.size(); ++a)
        for (size_t b = a + 1; b < blocks.size(); ++b)
            CHECK(!overlap(blocks[a], blocks[b]));

    const R band{0, 0, l.width, l.header.bandHeight};
    for (const auto& blk : blocks)
        CHECK(!overlap(blk, band));
}

void groups_and_master_inside_window()
{
    const auto l = bl::computeLayout();
    const R win{0, 0, l.width, l.height};
    CHECK(inside(r(l.atk), win));
    CHECK(inside(r(l.sus), win));
    CHECK(inside(r(l.glob), win));
    CHECK(inside(r(l.master), win));
}

void content_starts_below_header_band()
{
    const auto l = bl::computeLayout();
    CHECK(l.atk.y >= l.header.bandHeight);
    CHECK(l.sus.y >= l.header.bandHeight);
    CHECK(l.glob.y >= l.header.bandHeight);
    CHECK(l.master.y >= l.header.bandHeight);
}

void knob_size_72()
{
    const auto l = bl::computeLayout();
    for (const auto& g : groups(l))
        for (int i = 0; i < g.count; ++i)
        {
            const auto& b = l.knob[g.params[static_cast<size_t>(i)]].body;
            CHECK(b.w == 72.f);
            CHECK(b.h == 72.f);
        }
}

void master_rightmost_full_height()
{
    const auto l = bl::computeLayout();
    CHECK(l.master.x + l.master.w == l.width - bl::kMargin);
    CHECK(l.master.w == hui::kMasterW);
    CHECK(l.master.y == l.atk.y);
    CHECK(l.master.y + l.master.h == l.height - bl::kMargin);
    CHECK(l.master.x >= l.glob.x + l.glob.w);
}

void width_at_least_header_min()
{
    const auto l = bl::computeLayout();
    CHECK(l.width >= hui::kHeaderMinWidth);
    CHECK(l.header.bandHeight == hui::computeHeaderLayout(l.width).bandHeight);
}

void output_gain_has_no_knob_slot()
{
    const auto l = bl::computeLayout();
    const auto& k = l.knob[bl::kOutputGain];
    CHECK(k.body.w == 0 && k.body.h == 0 && k.label.w == 0 && k.label.h == 0);
}

void fader_param_routing()
{
    int count = 0;
    for (uint32_t i = 0; i < bl::kParamCount; ++i)
    {
        if (bl::isFaderParam(i))
        {
            ++count;
            CHECK(i == bl::kOutputGain);
        }
    }
    CHECK(count == 1);
    CHECK(bl::kParamCount == 13);
    CHECK(!bl::isFaderParam(bl::kParamCount));
}

} // namespace

int main()
{
    all_knobs_inside_their_group();
    no_overlap();
    groups_and_master_inside_window();
    content_starts_below_header_band();
    knob_size_72();
    master_rightmost_full_height();
    width_at_least_header_min();
    output_gain_has_no_knob_slot();
    fader_param_routing();
    TEST_SUMMARY();
    return 0;
}
