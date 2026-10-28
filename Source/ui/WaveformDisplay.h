#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class GeverbAudioProcessor;

namespace geverb::ui
{
class WaveformDisplay final : public juce::Component,
                              private juce::Timer
{
public:
    explicit WaveformDisplay (GeverbAudioProcessor&);
    ~WaveformDisplay() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintScreen (juce::Graphics&, juce::Rectangle<float>) const;
    void paintLegend (juce::Graphics&, juce::Rectangle<float>) const;
    void paintTailMeter (juce::Graphics&, juce::Rectangle<float>) const;

    GeverbAudioProcessor& processor;

    float displayGain = 1.0f;
    float displayedTail = 0.0f;
    float phase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
} // namespace geverb::ui
