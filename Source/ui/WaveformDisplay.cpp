#include "WaveformDisplay.h"

#include "Parameters.h"
#include "PluginProcessor.h"
#include "Theme.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace geverb::ui
{
namespace
{
constexpr int displayPoints = 1024;

float readWet (const GeverbAudioProcessor& processor, uint32_t index)
{
    return processor.getWetWaveformSample (index);
}

float readInput (const GeverbAudioProcessor& processor, uint32_t index)
{
    return processor.getInputWaveformSample (index);
}
} // namespace

WaveformDisplay::WaveformDisplay (GeverbAudioProcessor& p)
    : processor (p)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
}

void WaveformDisplay::resized() {}

void WaveformDisplay::timerCallback()
{
    // A slowly breathing display gain keeps quiet tails visible without
    // turning the screen into a noise magnifier.
    const uint32_t write = processor.getWaveformWriteIndex();
    float peak = 0.0f;

    for (int i = 0; i < displayPoints; ++i)
        peak = std::max (peak, std::abs (readWet (processor, write - static_cast<uint32_t> (displayPoints - i))));

    const float targetGain = peak > 0.0008f ? juce::jlimit (1.0f, 10.0f, 0.68f / peak) : 1.0f;
    displayGain += 0.12f * (targetGain - displayGain);

    const float targetTail = processor.getTailEnergy();
    displayedTail += 0.18f * (targetTail - displayedTail);

    phase += 0.045f;

    if (phase > juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;

    repaint();
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    constexpr float corner = 6.0f;

    // Recessed obsidian display under the glass.
    juce::ColourGradient screenGradient (obsidianScreen.brighter (0.025f), bounds.getTopLeft(),
                                         obsidianScreen.darker (0.16f), bounds.getBottomRight(), false);
    g.setGradientFill (screenGradient);
    g.fillRoundedRectangle (bounds, corner);

    paintScreen (g, bounds.reduced (2.0f));
    paintLegend (g, bounds.reduced (2.0f));
    paintTailMeter (g, bounds.reduced (2.0f));

    // Polished recessed bezel.
    g.setColour (crimsonAccent.withAlpha (0.42f));
    g.drawRoundedRectangle (bounds, corner, 1.05f);
    g.setColour (glassRimHighlight.withAlpha (0.18f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), corner - 1.0f, 0.8f);

    // Upper-left soft-box reflection over the screen.
    juce::Graphics::ScopedSaveState state (g);
    juce::Path clip;
    clip.addRoundedRectangle (bounds, corner);
    g.reduceClipRegion (clip);

    juce::Path sheen;
    sheen.startNewSubPath (bounds.getX(), bounds.getY());
    sheen.lineTo (bounds.getX() + bounds.getWidth() * 0.40f, bounds.getY());
    sheen.lineTo (bounds.getX() + bounds.getWidth() * 0.12f, bounds.getBottom());
    sheen.lineTo (bounds.getX(), bounds.getBottom());
    sheen.closeSubPath();
    g.setColour (juce::Colours::white.withAlpha (0.028f));
    g.fillPath (sheen);
}

void WaveformDisplay::paintScreen (juce::Graphics& g, juce::Rectangle<float> screen) const
{
    auto inner = screen;

    inner.removeFromTop (23.0f);
    inner.removeFromBottom (17.0f);

    auto plot = inner.reduced (7.0f, 3.0f);
    const auto labelColumn = plot.removeFromLeft (30.0f);
    plot.removeFromRight (8.0f);

    if (plot.getWidth() < 20.0f || plot.getHeight() < 20.0f)
        return;

    // A soft crimson field behind the trace, breathing with the tail energy.
    const float energy = juce::jlimit (0.0f, 1.0f, displayedTail * 4.0f);
    const auto centre = plot.getCentre();
    juce::ColourGradient aura (crimsonGlow.withAlpha (0.055f + 0.10f * energy), centre,
                               juce::Colours::transparentBlack,
                               centre.translated (plot.getWidth() * 0.48f, 0.0f), true);
    g.setGradientFill (aura);
    g.fillRect (plot);

    // dB scale grid.
    constexpr int numHorizontal = 5;
    const char* labels[numHorizontal] { "0 dB", "-6 dB", "-12 dB", "-18 dB", "-24 dB" };

    g.setFont (font (6.2f, FontWeight::regular, 0.05f));

    for (int i = 0; i < numHorizontal; ++i)
    {
        const float prop = static_cast<float> (i + 1) / static_cast<float> (numHorizontal + 1);
        const float y = plot.getY() + prop * plot.getHeight();

        g.setColour (i == 1 ? obsidianGrid.brighter (0.10f) : obsidianGrid);
        g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);

        g.setColour (textMuted.withAlpha (0.72f));
        g.drawText (labels[i],
                    labelColumn.withY (y - 4.0f).withHeight (8.0f),
                    juce::Justification::centredRight, false);
    }

    constexpr int numVertical = 8;

    for (int i = 1; i < numVertical; ++i)
    {
        const float prop = static_cast<float> (i) / static_cast<float> (numVertical);
        const float x = plot.getX() + prop * plot.getWidth();
        g.setColour (obsidianGrid.withAlpha (0.85f));
        g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
    }

    const uint32_t write = processor.getWaveformWriteIndex();
    const float midY = plot.getCentreY();
    const float halfH = plot.getHeight() * 0.43f;
    constexpr int numColumns = 256;
    constexpr int samplesPerColumn = displayPoints / numColumns;

    std::array<float, numColumns> columnMax {};
    std::array<float, numColumns> columnMin {};
    std::array<float, numColumns> dryMax {};
    std::array<float, numColumns> dryMin {};
    float peak = 0.0f;
    float dryPeak = 0.0f;

    for (int c = 0; c < numColumns; ++c)
    {
        float high = -1.0f;
        float low = 1.0f;
        float dryHigh = -1.0f;
        float dryLow = 1.0f;

        const auto columnAge = static_cast<uint32_t> ((numColumns - c - 1) * samplesPerColumn);

        for (int k = 0; k < samplesPerColumn; ++k)
        {
            const auto index = write - columnAge - static_cast<uint32_t> (samplesPerColumn - 1 - k);
            const float wetSample = readWet (processor, index);
            const float drySample = readInput (processor, index);
            high = std::max (high, wetSample);
            low = std::min (low, wetSample);
            dryHigh = std::max (dryHigh, drySample);
            dryLow = std::min (dryLow, drySample);
            peak = std::max (peak, std::abs (wetSample));
            dryPeak = std::max (dryPeak, std::abs (drySample));
        }

        columnMax[static_cast<size_t> (c)] = high;
        columnMin[static_cast<size_t> (c)] = low;
        dryMax[static_cast<size_t> (c)] = dryHigh;
        dryMin[static_cast<size_t> (c)] = dryLow;
    }

    if (peak > 0.000001f)
    {
        // Quiet ghost of the dry guitar to show attack against the tail.
        if (dryPeak > 0.0004f)
        {
            juce::Path dryTop;
            juce::Path dryBottom;
            const float dryGain = 0.20f / std::max (0.08f, dryPeak);

            for (int c = 0; c < numColumns; ++c)
            {
                const float x = plot.getX() + static_cast<float> (c)
                                           / static_cast<float> (numColumns - 1) * plot.getWidth();
                const float yTop = midY - juce::jlimit (-1.0f, 1.0f, dryMax[static_cast<size_t> (c)] * dryGain) * halfH;
                const float yBottom = midY - juce::jlimit (-1.0f, 1.0f, dryMin[static_cast<size_t> (c)] * dryGain) * halfH;

                if (c == 0)
                {
                    dryTop.startNewSubPath (x, yTop);
                    dryBottom.startNewSubPath (x, yBottom);
                }
                else
                {
                    dryTop.lineTo (x, yTop);
                    dryBottom.lineTo (x, yBottom);
                }
            }

            g.setColour (textSecondary.withAlpha (0.09f));
            g.strokePath (dryTop, juce::PathStrokeType (0.7f));
            g.strokePath (dryBottom, juce::PathStrokeType (0.7f));
        }

        juce::Path fillPath;
        juce::Path topTrace;
        juce::Path bottomTrace;

        auto yFor = [&] (float sample)
        {
            return midY - juce::jlimit (-1.0f, 1.0f, sample * displayGain) * halfH;
        };

        for (int c = 0; c < numColumns; ++c)
        {
            const float x = plot.getX() + static_cast<float> (c)
                                       / static_cast<float> (numColumns - 1) * plot.getWidth();
            const float y = yFor (columnMax[static_cast<size_t> (c)]);

            if (c == 0)
            {
                fillPath.startNewSubPath (x, y);
                topTrace.startNewSubPath (x, y);
            }
            else
            {
                fillPath.lineTo (x, y);
                topTrace.lineTo (x, y);
            }
        }

        for (int c = numColumns - 1; c >= 0; --c)
        {
            const float x = plot.getX() + static_cast<float> (c)
                                       / static_cast<float> (numColumns - 1) * plot.getWidth();
            const float y = yFor (columnMin[static_cast<size_t> (c)]);
            fillPath.lineTo (x, y);

            if (c == numColumns - 1)
                bottomTrace.startNewSubPath (x, y);
            else
                bottomTrace.lineTo (x, y);
        }

        fillPath.closeSubPath();

        juce::ColourGradient envelope (crimsonAccent.withAlpha (0.24f), plot.getX(), midY - halfH,
                                       crimsonAccent.withAlpha (0.24f), plot.getX(), midY + halfH, false);
        envelope.addColour (0.5, crimsonGlow.withAlpha (0.12f));
        g.setGradientFill (envelope);
        g.fillPath (fillPath);

        g.setColour (crimsonGlow.withAlpha (0.20f));
        g.strokePath (topTrace, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        g.strokePath (bottomTrace, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));

        g.setColour (crimsonAccent.withAlpha (0.90f));
        g.strokePath (topTrace, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        g.strokePath (bottomTrace, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));

        g.setColour (crimsonBright.withAlpha (0.90f));
        g.strokePath (topTrace, juce::PathStrokeType (0.75f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour (textMuted.withAlpha (0.32f));
        g.setFont (font (7.0f, FontWeight::semibold, 0.20f));
        g.drawText ("AWAITING GUITAR", plot, juce::Justification::centred, false);
    }

    // Tiny centre-line glint.
    g.setColour (crimsonAccent.withAlpha (0.16f));
    g.fillRect (plot.getX(), midY, plot.getWidth(), 0.6f);
}

void WaveformDisplay::paintLegend (juce::Graphics& g, juce::Rectangle<float> screen) const
{
    const auto* state = &processor.getValueTreeState();
    const int mode = juce::roundToInt (state->getRawParameterValue (geverb::params::mode)->load());
    const float decay = state->getRawParameterValue (geverb::params::decay)->load();
    const float predelay = state->getRawParameterValue (geverb::params::predelay)->load();

    auto top = screen.removeFromTop (23.0f);
    top.reduce (8.0f, 0.0f);

    const float pulse = 0.65f + 0.35f * std::sin (phase);
    drawCrimsonDot (g, { top.getX() + 4.0f, top.getCentreY() }, 1.45f + 0.25f * pulse);

    g.setColour (textSecondary.withAlpha (0.90f));
    g.setFont (font (7.0f, FontWeight::semibold, 0.19f));
    g.drawText ("TAIL FIELD", top.withTrimmedLeft (14.0f).withWidth (140.0f),
                juce::Justification::centredLeft, false);

    g.setColour (textMuted.withAlpha (0.82f));
    g.setFont (font (6.2f, FontWeight::semibold, 0.13f));
    g.drawText ("REAL-TIME REVERB RETURN",
                top.withTrimmedLeft (94.0f).withWidth (200.0f),
                juce::Justification::centredLeft, false);

    const juce::String right = formatMode (mode) + "   /   " + formatSeconds (decay)
                             + "   /   " + formatMs (predelay);

    g.setColour (textSecondary.withAlpha (0.92f));
    g.setFont (font (7.2f, FontWeight::semibold, 0.15f));
    g.drawText (right, top, juce::Justification::centredRight, false);
}

void WaveformDisplay::paintTailMeter (juce::Graphics& g, juce::Rectangle<float> screen) const
{
    auto bottom = screen.removeFromBottom (17.0f);
    bottom.reduce (8.0f, 4.0f);

    g.setColour (textMuted.withAlpha (0.80f));
    g.setFont (font (6.0f, FontWeight::semibold, 0.16f));
    g.drawText ("TAIL", bottom.withWidth (26.0f), juce::Justification::centredLeft, false);

    const float db = juce::Decibels::gainToDecibels (displayedTail, -60.0f);
    const float level = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    const auto bar = juce::Rectangle<float> (bottom.getX() + 30.0f, bottom.getCentreY() - 1.0f, 150.0f, 2.0f);

    g.setColour (obsidianGrid);
    g.fillRoundedRectangle (bar, 1.0f);
    g.setColour (crimsonAccent.withAlpha (0.78f));
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * level), 1.0f);
    g.setColour (crimsonBright.withAlpha (0.55f));
    g.fillRect (bar.getX() + bar.getWidth() * level - 1.0f, bar.getY() - 1.0f, 1.0f, 4.0f);

    g.setColour (textMuted.withAlpha (0.72f));
    g.setFont (font (5.8f, FontWeight::regular, 0.10f));
    g.drawText (juce::String (db, 1) + " dB",
                bottom.withTrimmedLeft (190.0f).withWidth (64.0f),
                juce::Justification::centredLeft, false);

    g.setColour (textMuted.withAlpha (0.65f));
    g.drawText ("LAST 1.4 S", bottom, juce::Justification::centredRight, false);
}

} // namespace geverb::ui
