#include "test_runner.h"
#include "../Source/FactoryPresets.h"

int main()
{
    const auto presets = havocFactoryPresets();
    CHECK(presets.size() == 3);

    for (const auto& p : presets)
    {
        CHECK(p.pluginId == "com.spellbound.havoc");
        CHECK(p.parameters.size() == 13);   // all 13 HAVOC_PARAM_* ids present
    }

    CHECK(presets[0].name == "Punchy Kick");
    CHECK(presets[1].name == "Sub Reinforce");
    CHECK(presets[2].name == "Snappy Transient");

    TEST_SUMMARY();
    return 0;
}
