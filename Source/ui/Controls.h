#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <functional>
#include <memory>

class GeverbAudioProcessor;

namespace geverb::ui
{
class ParamKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String (float)>;

    ParamKnob (juce::AudioProcessorValueTreeState& state,
               const juce::String& parameterID,
               const juce::String& displayName,
               Formatter formatter);

    void resized() override;
    void refreshValue();

private:
    juce::Slider slider;
    juce::Label nameLabel;
    juce::Label valueLabel;
    Formatter valueFormatter;
    juce::RangedAudioParameter* parameter = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamKnob)
};

class ParamSlider final : public juce::Component
{
public:
    using Formatter = std::function<juce::String (float)>;

    ParamSlider (juce::AudioProcessorValueTreeState& state,
                 const juce::String& parameterID,
                 const juce::String& displayName,
                 Formatter formatter);

    void resized() override;
    void refreshValue();

private:
    juce::Slider slider;
    juce::Label nameLabel;
    juce::Label valueLabel;
    Formatter valueFormatter;
    juce::RangedAudioParameter* parameter = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamSlider)
};

class OutputMeter final : public juce::Component,
                          private juce::Timer
{
public:
    explicit OutputMeter (GeverbAudioProcessor&);
    ~OutputMeter() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    static float dbToNormalised (float db) noexcept;

    GeverbAudioProcessor& processor;
    float displayLeftDb = -60.0f;
    float displayRightDb = -60.0f;
    float peakHoldDb = -60.0f;
    int peakHoldTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputMeter)
};

class TactileButton final : public juce::Button
{
public:
    TactileButton (juce::AudioProcessorValueTreeState& state,
                   const juce::String& parameterID,
                   juce::String label);

    bool isOn() const noexcept { return on; }

    void paintButton (juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
    using juce::Button::clicked;
    void clicked() override;

private:
    void setOn (bool shouldBeOn);

    juce::RangedAudioParameter* parameter = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::String label;
    bool on = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TactileButton)
};

class ModeSelector final : public juce::Component
{
public:
    ModeSelector (juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterID);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> segmentBounds (int index) const;
    void setSelected (int newIndex);
    int indexAt (juce::Point<float> position) const;

    juce::RangedAudioParameter* parameter = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::StringArray choices;
    int selected = 1;
    int hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModeSelector)
};

class PresetPill final : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    explicit PresetPill (GeverbAudioProcessor&);

    void refresh();
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void stepPreset (int direction);

    GeverbAudioProcessor& processor;
    int lastPreset = -1;
    bool lastDirty = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetPill)
};

} // namespace geverb::ui
