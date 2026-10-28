#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace geverb::ui
{
class StudioLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    StudioLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};
} // namespace geverb::ui
