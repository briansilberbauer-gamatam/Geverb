#include "PluginEditor.h"

#include "Parameters.h"
#include "Presets.h"
#include "Version.h"
#include "ui/Controls.h"
#include "ui/Theme.h"
#include "ui/WaveformDisplay.h"

#include <array>
#include <cmath>

using namespace geverb::ui;

namespace
{
constexpr int designWidth = 1180;
constexpr int designHeight = 720;

struct Layout
{
    juce::Rectangle<int> inner;
    juce::Rectangle<int> main;
    juce::Rectangle<int> mixer;
    juce::Rectangle<int> mixerUpper;
    juce::Rectangle<int> outputMeter;
    juce::Rectangle<int> header;
    juce::Rectangle<int> visual;
    juce::Rectangle<int> bottom;
    juce::Rectangle<int> modeRow;
};

Layout computeLayout (juce::Rectangle<int> full)
{
    Layout layout;

    auto unit = full.reduced (18, 16);
    auto metal = unit.reduced (5);
    auto glass = metal.reduced (6);
    auto inner = glass.reduced (18, 16);

    layout.inner = inner;
    layout.mixer = inner.removeFromRight (204);
    inner.removeFromRight (14);
    layout.main = inner;

    auto rightMixerBay = layout.mixer;
    layout.outputMeter = rightMixerBay.removeFromBottom (72).reduced (6, 4);
    rightMixerBay.removeFromBottom (8);
    layout.mixerUpper = rightMixerBay;

    layout.header = layout.main.removeFromTop (38);
    layout.bottom = layout.main.removeFromBottom (226);
    layout.main.removeFromBottom (10);
    layout.visual = layout.main;
    layout.modeRow = layout.bottom.removeFromTop (30);

    return layout;
}

juce::String percentValue (float value)
{
    return formatPercent (value);
}
} // namespace

class GeverbEditorContent final : public juce::Component,
                                  private juce::Timer
{
public:
    explicit GeverbEditorContent (GeverbAudioProcessor& p)
        : processor (p),
          apvts (p.getValueTreeState())
    {
        setOpaque (true);

        const std::array<const char*, 6> spaceIDs {
            geverb::params::decay, geverb::params::predelay, geverb::params::size,
            geverb::params::damping, geverb::params::shimmer, geverb::params::modulation
        };
        const std::array<const char*, 6> spaceNames {
            "DECAY", "PRE-DELAY", "SIZE", "DAMPING", "SHIMMER", "MOD"
        };
        const std::array<ParamKnob::Formatter, 6> spaceFormat {
            [] (float v) { return formatSeconds (v); },
            [] (float v) { return formatMs (v); },
            [] (float v) { return formatSize (v); },
            [] (float v) { return percentValue (v); },
            [] (float v) { return percentValue (v); },
            [] (float v) { return percentValue (v); }
        };

        for (size_t i = 0; i < spaceKnobs.size(); ++i)
        {
            spaceKnobs[i] = std::make_unique<ParamKnob> (apvts, spaceIDs[i], spaceNames[i], spaceFormat[i]);
            addAndMakeVisible (*spaceKnobs[i]);
        }

        inputSlider = std::make_unique<ParamSlider> (apvts, geverb::params::input, "INPUT",
                                                      [] (float v) { return formatDb (v); });
        mixSlider = std::make_unique<ParamSlider> (apvts, geverb::params::mix, "MIX",
                                                    [] (float v) { return formatPercent (v); });
        outputSlider = std::make_unique<ParamSlider> (apvts, geverb::params::output, "OUTPUT",
                                                       [] (float v) { return formatDb (v); });
        addAndMakeVisible (*inputSlider);
        addAndMakeVisible (*mixSlider);
        addAndMakeVisible (*outputSlider);

        outputMeter = std::make_unique<OutputMeter> (processor);
        addAndMakeVisible (*outputMeter);

        modeSelector = std::make_unique<ModeSelector> (apvts, geverb::params::mode);
        addAndMakeVisible (*modeSelector);

        holdButton = std::make_unique<TactileButton> (apvts, geverb::params::hold, "HOLD");
        bypassButton = std::make_unique<TactileButton> (apvts, geverb::params::bypass, "BYPASS");
        addAndMakeVisible (*holdButton);
        addAndMakeVisible (*bypassButton);

        waveform = std::make_unique<WaveformDisplay> (processor);
        addAndMakeVisible (*waveform);

        presetPill = std::make_unique<PresetPill> (processor);
        addAndMakeVisible (*presetPill);

        startTimerHz (20);
    }

    ~GeverbEditorContent() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        drawDesktopHardwareChassis (g, getLocalBounds().toFloat());

        const auto layout = computeLayout (getLocalBounds());
        paintHeader (g, layout);
        paintMixerBay (g, layout);
        paintBottomDeck (g, layout);
    }

    void resized() override
    {
        const auto layout = computeLayout (getLocalBounds());

        presetPill->setBounds (layout.header.withSizeKeepingCentre (360, 35));

        waveform->setBounds (layout.visual);

        auto mixer = layout.mixerUpper.reduced (1, 0);
        const int bayColumn = mixer.getWidth() / 3;

        for (int i = 0; i < 3; ++i)
        {
            auto column = mixer.removeFromLeft (i == 2 ? mixer.getWidth() : bayColumn);
            column.reduce (3, 0);

            if (i == 0)
                inputSlider->setBounds (column);
            else if (i == 1)
                mixSlider->setBounds (column);
            else
                outputSlider->setBounds (column);
        }

        auto modeRow = layout.modeRow;
        modeRow.removeFromLeft (58);

        auto utility = modeRow.removeFromRight (214);
        bypassButton->setBounds (utility.removeFromRight (104).reduced (0, 1));
        utility.removeFromRight (6);
        holdButton->setBounds (utility.removeFromRight (104).reduced (0, 1));

        modeSelector->setBounds (modeRow.removeFromLeft (318).reduced (0, 1));

        outputMeter->setBounds (layout.outputMeter);

        auto knobRow = layout.bottom;
        knobRow.removeFromTop (38);

        constexpr int gap = 8;
        const int columnWidth = (knobRow.getWidth() - gap * 5) / 6;

        for (int i = 0; i < 6; ++i)
        {
            auto column = knobRow.removeFromLeft (i == 5 ? knobRow.getWidth() : columnWidth);
            spaceKnobs[static_cast<size_t> (i)]->setBounds (column.reduced (1, 0));

            if (i < 5)
                knobRow.removeFromLeft (gap);
        }
    }

private:
    void timerCallback() override
    {
        presetPill->refresh();

        for (auto& knob : spaceKnobs)
            knob->refreshValue();

        inputSlider->refreshValue();
        mixSlider->refreshValue();
        outputSlider->refreshValue();
    }

    void paintHeader (juce::Graphics& g, const Layout& layout)
    {
        const auto header = layout.header;

        // Compact reverb mark: three glowing space bars.
        const std::array<float, 3> heights { 16.0f, 24.0f, 19.0f };

        for (size_t i = 0; i < heights.size(); ++i)
        {
            const float x = static_cast<float> (header.getX() + 1)
                          + static_cast<float> (i) * 7.0f;
            const float top = static_cast<float> (header.getY() + 27)
                            - heights[i];
            const auto bar = juce::Rectangle<float> (x, top, 3.8f, heights[i]);

            g.setColour (crimsonGlow.withAlpha (0.34f));
            g.fillRoundedRectangle (bar.expanded (1.2f, 0.8f), 3.0f);
            g.setColour (crimsonAccent);
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (crimsonBright.withAlpha (0.85f));
            g.fillRoundedRectangle (bar.reduced (0.8f, 1.3f), 1.0f);
        }

        g.setColour (textPrimary);
        g.setFont (font (21.0f, FontWeight::black, 0.04f));
        g.drawText ("GEVERB",
                    header.getX() + 31, header.getY() + 0, 230, 26,
                    juce::Justification::centredLeft, false);

        g.setColour (textSecondary);
        g.setFont (font (6.4f, FontWeight::semibold, 0.17f));
        g.drawText ("AMBIENT REVERB  /  LEAD GUITAR",
                    header.getX() + 32, header.getY() + 23, 240, 10,
                    juce::Justification::centredLeft, false);

        g.setColour (textPrimary.withAlpha (0.92f));
        g.setFont (font (6.9f, FontWeight::black, 0.16f));
        g.drawText ("BLS AUDIO",
                    header.getRight() - 170, header.getY() + 1, 170, 10,
                    juce::Justification::centredRight, false);

        g.setColour (textMuted);
        g.setFont (font (6.1f, FontWeight::semibold, 0.13f));
        g.drawText ("SPACE SERIES  /  v" + juce::String (geverb::version),
                    header.getRight() - 170, header.getY() + 12, 170, 9,
                    juce::Justification::centredRight, false);

        g.setColour (textSecondary.withAlpha (0.48f));
        g.drawText ("STEREO  /  32-BIT FLOAT",
                    header.getRight() - 170, header.getY() + 23, 170, 9,
                    juce::Justification::centredRight, false);

        g.setColour (obsidianGrid.withAlpha (0.9f));
        g.fillRect (header.getX(), header.getBottom() + 2, header.getWidth(), 1);
    }

    void paintMixerBay (juce::Graphics& g, const Layout& layout)
    {
        const auto bay = layout.mixer.toFloat();

        g.setColour (obsidianScreen.withAlpha (0.24f));
        g.fillRoundedRectangle (bay.reduced (1.0f), 6.0f);

        g.setColour (glassRimHighlight.withAlpha (0.08f));
        g.drawRoundedRectangle (bay.reduced (1.5f), 5.5f, 0.8f);

        // Full-height divider and crimson edge refraction.
        const float dividerX = bay.getX() - 7.0f;
        g.setColour (obsidianGrid);
        g.fillRect (dividerX, bay.getY() + 4.0f, 1.0f, bay.getHeight() - 8.0f);

        juce::ColourGradient dividerGlow (juce::Colours::transparentBlack, dividerX, bay.getY(),
                                          juce::Colours::transparentBlack, dividerX, bay.getBottom(), false);
        dividerGlow.addColour (0.5, crimsonAccent.withAlpha (0.34f));
        g.setGradientFill (dividerGlow);
        g.fillRect (dividerX + 1.0f, bay.getY() + 12.0f, 1.0f, bay.getHeight() - 24.0f);

        // Three slender vertical wells behind the mixer sliders. The lower
        // section of the bay is reserved for the vertical output meter.
        auto columns = layout.mixerUpper.reduced (1, 0);
        const int columnWidth = columns.getWidth() / 3;

        for (int i = 0; i < 3; ++i)
        {
            auto column = columns.removeFromLeft (i == 2 ? columns.getWidth() : columnWidth).toFloat();
            const auto well = column.reduced (7.0f, 40.0f);

            g.setColour (obsidianWell.withAlpha (0.20f));
            g.fillRoundedRectangle (well, 4.0f);

            if (i < 2)
            {
                g.setColour (obsidianGrid.withAlpha (0.42f));
                g.fillRect (column.getRight(), column.getY() + 44.0f, 1.0f, column.getHeight() - 70.0f);
            }
        }

        // Glass seam between the mixer faders and the output monitor.
        const float seamY = static_cast<float> (layout.outputMeter.getY()) - 4.0f;
        g.setColour (obsidianGrid.withAlpha (0.82f));
        g.fillRect (bay.getX() + 9.0f, seamY, bay.getWidth() - 18.0f, 1.0f);
        g.setColour (crimsonAccent.withAlpha (0.30f));
        g.fillRect (bay.getCentreX() - 18.0f, seamY, 36.0f, 1.0f);
    }

    void paintBottomDeck (juce::Graphics& g, const Layout& layout)
    {
        const auto deck = layout.bottom.toFloat().reduced (1.0f);

        g.setColour (obsidianScreen.withAlpha (0.14f));
        g.fillRoundedRectangle (deck, 7.0f);

        g.setColour (glassRimHighlight.withAlpha (0.07f));
        g.drawRoundedRectangle (deck, 7.0f, 0.8f);

        auto modeRow = layout.modeRow;
        const auto modeLabel = modeRow.removeFromLeft (58);

        g.setColour (textSecondary);
        g.setFont (font (7.4f, FontWeight::semibold, 0.18f));
        g.drawText ("VOICE",
                    modeLabel.withTrimmedTop (2).withHeight (12),
                    juce::Justification::centredLeft, false);

        g.setColour (textMuted);
        g.setFont (font (5.7f, FontWeight::regular, 0.11f));
        g.drawText ("TYPE",
                    modeLabel.withTrimmedTop (14).withHeight (9),
                    juce::Justification::centredLeft, false);

        // A quiet descriptor between the voice selector and utility buttons.
        const auto descriptor = juce::Rectangle<int> (modeLabel.getRight() + 332,
                                                       layout.modeRow.getY() + 3,
                                                       juce::jmax (0, layout.modeRow.getWidth() - 612),
                                                       22);

        if (descriptor.getWidth() > 40)
        {
            g.setColour (textMuted.withAlpha (0.75f));
            g.setFont (font (5.9f, FontWeight::semibold, 0.12f));
            g.drawText ("LONG DECAY  /  SOFT ATTACK  /  NO EDGES",
                        descriptor, juce::Justification::centredRight, false);
        }
    }

    GeverbAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    std::array<std::unique_ptr<ParamKnob>, 6> spaceKnobs;
    std::unique_ptr<ParamSlider> inputSlider;
    std::unique_ptr<ParamSlider> mixSlider;
    std::unique_ptr<ParamSlider> outputSlider;
    std::unique_ptr<OutputMeter> outputMeter;
    std::unique_ptr<ModeSelector> modeSelector;
    std::unique_ptr<TactileButton> holdButton;
    std::unique_ptr<TactileButton> bypassButton;
    std::unique_ptr<WaveformDisplay> waveform;
    std::unique_ptr<PresetPill> presetPill;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeverbEditorContent)
};

//==============================================================================
GeverbAudioProcessorEditor::GeverbAudioProcessorEditor (GeverbAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    content = std::make_unique<GeverbEditorContent> (processor);
    addAndMakeVisible (*content);

    setResizable (true, true);
    setResizeLimits (885, 540, 1770, 1080);

    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio (static_cast<double> (designWidth)
                                          / static_cast<double> (designHeight));

    setSize (designWidth, designHeight);
}

GeverbAudioProcessorEditor::~GeverbAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void GeverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff07080b));
}

void GeverbAudioProcessorEditor::resized()
{
    if (content == nullptr)
        return;

    const float scale = juce::jmin (static_cast<float> (getWidth()) / static_cast<float> (designWidth),
                                    static_cast<float> (getHeight()) / static_cast<float> (designHeight));
    const float offsetX = (static_cast<float> (getWidth()) - static_cast<float> (designWidth) * scale) * 0.5f;
    const float offsetY = (static_cast<float> (getHeight()) - static_cast<float> (designHeight) * scale) * 0.5f;

    content->setBounds (0, 0, designWidth, designHeight);
    content->setTransform (juce::AffineTransform::scale (scale).translated (offsetX, offsetY));
}
