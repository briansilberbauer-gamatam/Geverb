#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "ui/StudioLookAndFeel.h"

#include <memory>

class GeverbEditorContent;

class GeverbAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit GeverbAudioProcessorEditor (GeverbAudioProcessor&);
    ~GeverbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GeverbAudioProcessor& processor;
    geverb::ui::StudioLookAndFeel lookAndFeel;
    std::unique_ptr<GeverbEditorContent> content;
    juce::TooltipWindow tooltipWindow { this, 650 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeverbAudioProcessorEditor)
};
