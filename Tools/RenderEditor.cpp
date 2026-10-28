#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Version.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <cmath>
#include <iostream>
#include <memory>

class GeverbRenderApplication final : public juce::JUCEApplication,
                                      private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "Geverb Screenshot"; }
    const juce::String getApplicationVersion() override { return geverb::version; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}
    void systemRequestedQuit() override {}
    void shutdown() override {}

    void initialise (const juce::String& commandLine) override
    {
        outputFile = juce::File::getCurrentWorkingDirectory().getChildFile ("geverb-ui.png");

        for (const auto& argument : juce::StringArray::fromTokens (commandLine, true))
        {
            if (argument.startsWith ("--out="))
            {
                const auto path = argument.substring (6);
                outputFile = juce::File::isAbsolutePath (path)
                                 ? juce::File (path)
                                 : juce::File::getCurrentWorkingDirectory().getChildFile (path);
            }
        }

        outputFile = outputFile.getFullPathName();
        outputFile.getParentDirectory().createDirectory();

        processor = std::make_unique<GeverbAudioProcessor>();
        processor->applyPreset (0);

        editor = std::unique_ptr<juce::AudioProcessorEditor> (processor->createEditor());

        if (editor == nullptr)
        {
            std::cerr << "Could not create the plugin editor.\n";
            setApplicationReturnValue (1);
            quit();
            return;
        }

        editor->setSize (1180, 720);
        editor->addToDesktop (0);
        editor->setTopLeftPosition (30, 30);
        editor->setVisible (true);
        editor->toFront (true);

        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition ({ 3000.0f, 3000.0f });

        processor->setPlayConfigDetails (2, 2, 48000.0, 1024);
        processor->prepareToPlay (48000.0, 1024);
        testBuffer.setSize (2, 1024);
        startTimerHz (30);

        juce::Timer::callAfterDelay (1600, [this]
        {
            stopTimer();
            juce::Desktop::getInstance().getMainMouseSource().setScreenPosition ({ 3000.0f, 3000.0f });

            auto hideTooltips = [&] (auto&& self, juce::Component& component) -> void
            {
                for (auto* child : component.getChildren())
                {
                    if (dynamic_cast<juce::TooltipWindow*> (child) != nullptr)
                        child->setVisible (false);
                    else
                        self (self, *child);
                }
            };
            hideTooltips (hideTooltips, *editor);

            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);

            if (image.isValid())
            {
                outputFile.deleteFile();

                if (auto stream = std::unique_ptr<juce::FileOutputStream> (outputFile.createOutputStream()))
                {
                    juce::PNGImageFormat png;
                    const bool written = png.writeImageToStream (image, *stream);
                    stream->flush();

                    if (written)
                        std::cout << "Rendered " << outputFile.getFullPathName() << "\n";
                    else
                        std::cerr << "Failed to encode PNG.\n";
                }
                else
                {
                    std::cerr << "Could not open " << outputFile.getFullPathName() << "\n";
                }
            }
            else
            {
                std::cerr << "Editor snapshot was invalid.\n";
            }

            processor->releaseResources();
            editor->removeFromDesktop();
            editor.reset();
            processor.reset();
            quit();
        });
    }

private:
    void timerCallback() override
    {
        if (processor == nullptr)
            return;

        for (int block = 0; block < 4; ++block)
        {
            for (int i = 0; i < testBuffer.getNumSamples(); ++i)
            {
                const double n = static_cast<double> (sampleCounter + i);
                const double t = n / 48000.0;
                const double notePhase = std::fmod (t + 0.2, 0.72);
                const double envelope = std::exp (-4.2 * notePhase)
                                      * (1.0 - std::exp (-260.0 * notePhase));

                const double phrase = std::fmod (std::floor (t / 0.72), 4.0);
                const double frequency = phrase < 1.0 ? 196.0
                                       : phrase < 2.0 ? 246.94
                                       : phrase < 3.0 ? 293.66
                                                      : 392.0;

                const double fundamental = juce::MathConstants<double>::twoPi * frequency * n / 48000.0;

                const float signal = static_cast<float> (0.34 * envelope
                    * (std::sin (fundamental)
                       + 0.38 * std::sin (fundamental * 2.0)
                       + 0.18 * std::sin (fundamental * 3.0)
                       + 0.08 * std::sin (fundamental * 4.0)));

                testBuffer.setSample (0, i, signal);
                testBuffer.setSample (1, i, signal * 0.96f);
            }

            sampleCounter += testBuffer.getNumSamples();
            juce::MidiBuffer midi;
            processor->processBlock (testBuffer, midi);
        }
    }

    std::unique_ptr<GeverbAudioProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::File outputFile;
    juce::AudioBuffer<float> testBuffer;
    juce::int64 sampleCounter = 0;
};

START_JUCE_APPLICATION (GeverbRenderApplication)
