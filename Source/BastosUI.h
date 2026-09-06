// Source/BastosUI.h
#pragma once

#include "DistrhoUI.hpp"

START_NAMESPACE_DISTRHO

class BastosUI : public UI
{
public:
    BastosUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {}

protected:
    void parameterChanged(uint32_t, float) override {}

    void onNanoDisplay() override
    {
        beginPath();
        rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
        fillColor(DGL_NAMESPACE::Color(0x14, 0x14, 0x20)); // kBg 0xff141420, see Task 4
        fill();
        closePath();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BastosUI)
};

END_NAMESPACE_DISTRHO
