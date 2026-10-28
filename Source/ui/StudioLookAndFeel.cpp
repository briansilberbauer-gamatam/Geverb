#include "StudioLookAndFeel.h"

#include <cmath>

namespace geverb::ui
{
StudioLookAndFeel::StudioLookAndFeel()
{
    setColour (juce::Label::textColourId, textPrimary);
    setColour (juce::Label::textWhenEditingColourId, textPrimary);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::Slider::rotarySliderOutlineColourId, obsidianWell);
    setColour (juce::Slider::rotarySliderFillColourId, crimsonAccent);
    setColour (juce::Slider::thumbColourId, crimsonAccent);
    setColour (juce::Slider::textBoxTextColourId, textPrimary);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    setColour (juce::TooltipWindow::backgroundColourId, obsidianWell);
    setColour (juce::TooltipWindow::textColourId, textPrimary);
    setColour (juce::TooltipWindow::outlineColourId, crimsonAccent.withAlpha (0.45f));
}

void StudioLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPosProportional, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (width), static_cast<float> (height))
                            .reduced (3.0f);

    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

    if (radius < 10.0f)
        return;

    const auto centre = bounds.getCentre();
    const float prop = juce::jlimit (0.0f, 1.0f, sliderPosProportional);
    const float angle = rotaryStartAngle + prop * (rotaryEndAngle - rotaryStartAngle);

    // Etched perimeter tick hairlines on the glass surface.
    const float scaleR = radius - 1.5f;

    for (int i = 0; i <= 24; ++i)
    {
        const float tProp = static_cast<float> (i) / 24.0f;
        const float tickAngle = rotaryStartAngle + tProp * (rotaryEndAngle - rotaryStartAngle);
        const bool isMajor = (i % 4 == 0);
        const auto p1 = centre.getPointOnCircumference (scaleR - (isMajor ? 4.0f : 2.2f), tickAngle);
        const auto p2 = centre.getPointOnCircumference (scaleR, tickAngle);
        const bool lit = tProp <= prop + 0.001f;

        g.setColour (lit ? crimsonAccent.withAlpha (isMajor ? 0.82f : 0.48f)
                         : textMuted.withAlpha (isMajor ? 0.50f : 0.24f));
        g.drawLine ({ p1, p2 }, isMajor ? 1.2f : 0.75f);
    }

    // Recessed obsidian well with a diffused crimson LED under-glow.
    const float wellR = radius * 0.86f;
    const juce::Rectangle<float> wellBounds (centre.x - wellR, centre.y - wellR,
                                             wellR * 2.0f, wellR * 2.0f);
    g.setColour (obsidianWell);
    g.fillEllipse (wellBounds);

    {
        juce::ColourGradient underGlow (crimsonAccent.withAlpha (0.10f + 0.24f * prop),
                                        centre.x, centre.y,
                                        juce::Colours::transparentBlack,
                                        centre.x + wellR, centre.y, true);
        g.setGradientFill (underGlow);
        g.fillEllipse (wellBounds.expanded (2.0f));
    }

    if (prop > 0.002f)
    {
        juce::Path underArc;
        underArc.addCentredArc (centre.x, centre.y, wellR - 1.4f, wellR - 1.4f,
                                0.0f, rotaryStartAngle, angle, true);
        g.setColour (crimsonGlow.withAlpha (0.50f));
        g.strokePath (underArc, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        g.setColour (crimsonAccent.withAlpha (0.90f));
        g.strokePath (underArc, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // Low-profile cast shadow under the machined rim.
    const float rimR = radius * 0.79f;
    const juce::Rectangle<float> rimBounds (centre.x - rimR, centre.y - rimR,
                                            rimR * 2.0f, rimR * 2.0f);
    g.setColour (dropShadowDeep.withAlpha (0.70f));
    g.fillEllipse (rimBounds.translated (1.5f, 2.2f));

    // Heavy-machined outer rim with 64 fine vertical knurled ridges.
    juce::ColourGradient rimGradient (gunmetalLight.brighter (0.16f), rimBounds.getTopLeft(),
                                      obsidianScreen, rimBounds.getBottomRight(), false);
    g.setGradientFill (rimGradient);
    g.fillEllipse (rimBounds);

    constexpr int numKnurls = 64;
    for (int i = 0; i < numKnurls; ++i)
    {
        const float knurlAngle = angle + static_cast<float> (i)
                                           * (juce::MathConstants<float>::twoPi / static_cast<float> (numKnurls));
        const auto k1 = centre.getPointOnCircumference (rimR * 0.865f, knurlAngle);
        const auto k2 = centre.getPointOnCircumference (rimR * 0.985f, knurlAngle);

        const float lightBias = 0.5f - 0.5f * std::sin (knurlAngle
                                                       + juce::MathConstants<float>::pi * 0.25f);

        g.setColour (i % 2 == 0
                         ? juce::Colours::white.withAlpha (0.055f + 0.16f * lightBias)
                         : juce::Colours::black.withAlpha (0.62f));
        g.drawLine ({ k1, k2 }, 0.9f);
    }

    // Flat low-profile brushed metal cap.
    const float capR = rimR * 0.84f;
    const juce::Rectangle<float> capBounds (centre.x - capR, centre.y - capR,
                                            capR * 2.0f, capR * 2.0f);
    juce::ColourGradient capGradient (gunmetalLight.brighter (0.08f), capBounds.getTopLeft(),
                                      gunmetalDeep, capBounds.getBottomRight(), false);
    capGradient.addColour (0.5, gunmetalBase);
    g.setGradientFill (capGradient);
    g.fillEllipse (capBounds);

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path capClip;
        capClip.addEllipse (capBounds);
        g.reduceClipRegion (capClip);

        for (int gy = static_cast<int> (capBounds.getY()); gy < static_cast<int> (capBounds.getBottom()); gy += 2)
        {
            g.setColour (juce::Colours::white.withAlpha (0.022f));
            g.fillRect (capBounds.getX(), static_cast<float> (gy), capBounds.getWidth(), 1.0f);
        }
    }

    g.setColour (glassRimHighlight.withAlpha (0.24f));
    g.drawEllipse (capBounds, 0.9f);
    g.setColour (obsidianScreen.withAlpha (0.70f));
    g.drawEllipse (capBounds.expanded (0.8f), 0.8f);

    // Tiny surgical cap tick mark indicating position.
    const auto tickInner = centre.getPointOnCircumference (capR * 0.64f, angle);
    const auto tickOuter = centre.getPointOnCircumference (capR * 0.88f, angle);
    g.setColour (crimsonGlow.withAlpha (0.50f));
    g.drawLine ({ tickInner, tickOuter }, 3.0f);
    g.setColour (textPrimary);
    g.drawLine ({ tickInner, tickOuter }, 1.5f);
}

void StudioLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float, float,
                                          juce::Slider::SliderStyle style, juce::Slider&)
{
    if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearBarVertical)
        return;

    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (width), static_cast<float> (height));
    const float centreX = bounds.getCentreX();
    const float trackTop = bounds.getY() + 7.0f;
    const float trackBot = bounds.getBottom() - 7.0f;
    const float trackH = juce::jmax (10.0f, trackBot - trackTop);
    const float thumbY = juce::jlimit (trackTop, trackBot, sliderPos);

    // Minimalist tick marks arrayed along both sides of the recessed track.
    constexpr int numTicks = 21;

    for (int i = 0; i < numTicks; ++i)
    {
        const float prop = static_cast<float> (i) / static_cast<float> (numTicks - 1);
        const float tickY = trackTop + prop * trackH;
        const bool isMajor = (i % 5 == 0);
        const float tickLen = isMajor ? 7.0f : 4.0f;
        const float gap = 6.5f;

        g.setColour (isMajor ? textSecondary.withAlpha (0.52f) : textMuted.withAlpha (0.34f));
        g.fillRect (centreX - gap - tickLen, tickY - 0.5f, tickLen, 1.0f);
        g.fillRect (centreX + gap, tickY - 0.5f, tickLen, 1.0f);
    }

    // Recessed ultra-thin vertical glass track.
    const juce::Rectangle<float> trackRect (centreX - 2.0f, trackTop, 4.0f, trackH);
    g.setColour (obsidianWell);
    g.fillRoundedRectangle (trackRect, 2.0f);
    g.setColour (glassRimHighlight.withAlpha (0.14f));
    g.drawRoundedRectangle (trackRect.expanded (0.5f), 2.2f, 0.7f);

    // Muted crimson level fill rising from the bottom of the track.
    const float fillH = trackBot - thumbY;

    if (fillH > 0.5f)
    {
        const juce::Rectangle<float> activeRect (centreX - 1.5f, thumbY, 3.0f, fillH);
        g.setColour (crimsonGlow.withAlpha (0.34f));
        g.fillRoundedRectangle (activeRect.expanded (1.5f, 0.5f), 2.0f);
        g.setColour (crimsonAccent);
        g.fillRoundedRectangle (activeRect, 1.5f);
    }

    // Ultra-thin flush machined touch bar.
    const juce::Rectangle<float> thumb (centreX - 11.0f, thumbY - 4.5f, 22.0f, 9.0f);
    g.setColour (dropShadowDeep.withAlpha (0.60f));
    g.fillRoundedRectangle (thumb.translated (0.8f, 1.4f), 2.0f);

    juce::ColourGradient thumbGradient (gunmetalLight.brighter (0.18f), thumb.getTopLeft(),
                                        gunmetalDeep, thumb.getBottomRight(), false);
    g.setGradientFill (thumbGradient);
    g.fillRoundedRectangle (thumb, 2.0f);

    g.setColour (glassRimHighlight.withAlpha (0.28f));
    g.drawRoundedRectangle (thumb, 2.0f, 0.8f);

    g.setColour (crimsonBright);
    g.fillRect (thumb.getX() + 3.0f, thumbY - 0.5f, thumb.getWidth() - 6.0f, 1.2f);
}
} // namespace geverb::ui
