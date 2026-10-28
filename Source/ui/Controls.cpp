#include "Controls.h"

#include "Parameters.h"
#include "Presets.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace geverb::ui
{
namespace
{
juce::String modeName (int index)
{
    return geverb::params::modeNames[static_cast<size_t> (juce::jlimit (0, 2, index))];
}
} // namespace

//==============================================================================
ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& state,
                      const juce::String& parameterID,
                      const juce::String& displayName,
                      Formatter formatter)
    : valueFormatter (std::move (formatter)),
      parameter (state.getParameter (parameterID))
{
    jassert (parameter != nullptr);

    nameLabel.setText (displayName, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (font (8.4f, FontWeight::semibold, 0.15f));
    nameLabel.setColour (juce::Label::textColourId, textSecondary);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (font (10.5f, FontWeight::semibold, 0.02f));
    valueLabel.setColour (juce::Label::textColourId, textPrimary.withAlpha (0.96f));
    valueLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (valueLabel);

    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.18f,
                                juce::MathConstants<float>::pi * 2.82f,
                                true);
    slider.setMouseDragSensitivity (210);
    slider.setTooltip (displayName);
    slider.setName (displayName);
    slider.setColour (juce::Slider::rotarySliderFillColourId, crimsonAccent);
    slider.setColour (juce::Slider::thumbColourId, crimsonAccent);
    addAndMakeVisible (slider);

    slider.textFromValueFunction = [this] (double value)
    {
        return valueFormatter (static_cast<float> (value));
    };

    slider.onValueChange = [this] { refreshValue(); };

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterID, slider);

    if (parameter != nullptr)
        slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

    refreshValue();
}

void ParamKnob::resized()
{
    auto area = getLocalBounds().reduced (1, 0);

    constexpr int nameHeight = 12;
    constexpr int valueHeight = 16;
    const int availableForKnob = juce::jmax (10, area.getHeight() - nameHeight - valueHeight);
    const int diameter = juce::jmax (10, juce::jmin (area.getWidth() - 8, availableForKnob - 4));

    auto knobArea = area.removeFromTop (availableForKnob);
    slider.setBounds (knobArea.withSizeKeepingCentre (diameter, diameter));
    nameLabel.setBounds (area.removeFromTop (nameHeight));
    valueLabel.setBounds (area.removeFromTop (valueHeight));
}

void ParamKnob::refreshValue()
{
    valueLabel.setText (valueFormatter (static_cast<float> (slider.getValue())),
                        juce::dontSendNotification);
}

//==============================================================================
ParamSlider::ParamSlider (juce::AudioProcessorValueTreeState& state,
                          const juce::String& parameterID,
                          const juce::String& displayName,
                          Formatter formatter)
    : valueFormatter (std::move (formatter)),
      parameter (state.getParameter (parameterID))
{
    jassert (parameter != nullptr);

    nameLabel.setText (displayName, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (font (7.8f, FontWeight::semibold, 0.15f));
    nameLabel.setColour (juce::Label::textColourId, textSecondary);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (font (8.6f, FontWeight::semibold, 0.01f));
    valueLabel.setColour (juce::Label::textColourId, textPrimary.withAlpha (0.96f));
    valueLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (valueLabel);

    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (180);
    slider.setTooltip (displayName);
    slider.setName (displayName);
    slider.setColour (juce::Slider::thumbColourId, crimsonAccent);
    slider.setColour (juce::Slider::trackColourId, crimsonAccent);
    addAndMakeVisible (slider);

    slider.textFromValueFunction = [this] (double value)
    {
        return valueFormatter (static_cast<float> (value));
    };

    slider.onValueChange = [this] { refreshValue(); };

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterID, slider);

    if (parameter != nullptr)
        slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

    refreshValue();
}

void ParamSlider::resized()
{
    auto area = getLocalBounds().reduced (3, 0);
    nameLabel.setBounds (area.removeFromTop (22));
    valueLabel.setBounds (area.removeFromBottom (22));
    slider.setBounds (area.reduced (1, 2));
}

void ParamSlider::refreshValue()
{
    valueLabel.setText (valueFormatter (static_cast<float> (slider.getValue())),
                        juce::dontSendNotification);
}

//==============================================================================
OutputMeter::OutputMeter (GeverbAudioProcessor& p)
    : processor (p)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

OutputMeter::~OutputMeter()
{
    stopTimer();
}

float OutputMeter::dbToNormalised (float db) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
}

void OutputMeter::timerCallback()
{
    const float leftTarget = juce::Decibels::gainToDecibels (processor.getOutputLevelLeft(), -60.0f);
    const float rightTarget = juce::Decibels::gainToDecibels (processor.getOutputLevelRight(), -60.0f);

    auto ballistic = [] (float current, float target)
    {
        const float coefficient = target > current ? 0.62f : 0.12f;
        return current + coefficient * (target - current);
    };

    displayLeftDb = ballistic (displayLeftDb, leftTarget);
    displayRightDb = ballistic (displayRightDb, rightTarget);

    const float instantPeak = std::max (leftTarget, rightTarget);

    if (instantPeak > peakHoldDb)
    {
        peakHoldDb = instantPeak;
        peakHoldTicks = 45;
    }
    else if (peakHoldTicks > 0)
    {
        --peakHoldTicks;
    }
    else
    {
        peakHoldDb = std::max (-60.0f, peakHoldDb - 0.55f);
    }

    repaint();
}

void OutputMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    constexpr float corner = 5.0f;

    juce::ColourGradient screenGradient (obsidianScreen.brighter (0.035f), bounds.getTopLeft(),
                                         obsidianScreen.darker (0.20f), bounds.getBottomRight(), false);
    g.setGradientFill (screenGradient);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (glassRimHighlight.withAlpha (0.18f));
    g.drawRoundedRectangle (bounds, corner, 0.8f);
    g.setColour (crimsonAccent.withAlpha (0.28f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), corner - 0.8f, 0.6f);

    auto inner = bounds.reduced (8.0f, 5.0f);

    if (inner.getHeight() < 40.0f || inner.getWidth() < 100.0f)
        return;

    // Header row: title on the left, peak readout on the right.
    auto headerRow = inner.removeFromTop (13.0f);

    g.setColour (textSecondary);
    g.setFont (font (7.8f, FontWeight::semibold, 0.12f));
    g.drawText ("OUT METER", headerRow, juce::Justification::centredLeft, false);

    const bool hot = peakHoldDb > -0.5f;
    const juce::String peakText = peakHoldDb <= -59.5f
        ? juce::String ("-INF dB")
        : juce::String (peakHoldDb, 1) + " dB";

    g.setColour (hot ? crimsonBright : textPrimary);
    g.setFont (font (8.2f, FontWeight::semibold, 0.03f));
    g.drawText (peakText, headerRow, juce::Justification::centredRight, false);

    inner.removeFromTop (2.0f);
    auto scaleRow = inner.removeFromBottom (10.0f);

    // Horizontal stereo (L / R) meter tracks, left (-60 dB) to right (0 dB).
    constexpr float labelW = 9.0f;
    const float barLeft = inner.getX() + labelW;
    const float barRight = inner.getRight() - 1.0f;
    const float barW = juce::jmax (20.0f, barRight - barLeft);
    constexpr float barH = 6.2f;
    const float midY = inner.getCentreY();

    const juce::Rectangle<float> leftTrack (barLeft, midY - barH - 1.0f, barW, barH);
    const juce::Rectangle<float> rightTrack (barLeft, midY + 1.0f, barW, barH);

    g.setColour (textMuted);
    g.setFont (font (6.2f, FontWeight::semibold, 0.02f));
    g.drawText ("L", juce::Rectangle<float> (inner.getX(), leftTrack.getY() - 1.0f, labelW, barH + 2.0f),
                juce::Justification::centredLeft, false);
    g.drawText ("R", juce::Rectangle<float> (inner.getX(), rightTrack.getY() - 1.0f, labelW, barH + 2.0f),
                juce::Justification::centredLeft, false);

    g.setColour (obsidianWell);
    g.fillRoundedRectangle (leftTrack, 2.0f);
    g.fillRoundedRectangle (rightTrack, 2.0f);
    g.setColour (glassRimHighlight.withAlpha (0.12f));
    g.drawRoundedRectangle (leftTrack.expanded (0.4f), 2.2f, 0.6f);
    g.drawRoundedRectangle (rightTrack.expanded (0.4f), 2.2f, 0.6f);

    auto drawChannelFill = [&] (juce::Rectangle<float> track, float levelDb)
    {
        const float norm = dbToNormalised (levelDb);
        const float fillW = norm * track.getWidth();

        if (fillW <= 0.5f)
            return;

        const auto fillRect = track.withWidth (fillW);

        g.setColour (crimsonGlow.withAlpha (0.35f));
        g.fillRoundedRectangle (fillRect.expanded (0.5f, 1.2f), 2.0f);

        juce::ColourGradient barGradient (crimsonAccent, track.getX(), fillRect.getCentreY(),
                                          crimsonBright, track.getRight(), fillRect.getCentreY(), false);
        g.setGradientFill (barGradient);
        g.fillRoundedRectangle (fillRect, 1.5f);

        g.setColour (crimsonBright);
        g.fillRect (fillRect.getRight() - 1.5f, fillRect.getY(), 1.5f, fillRect.getHeight());
    };

    drawChannelFill (leftTrack, displayLeftDb);
    drawChannelFill (rightTrack, displayRightDb);

    // Ballistic peak-hold vertical marker across both horizontal bars.
    if (peakHoldDb > -59.0f)
    {
        const float peakX = barLeft + dbToNormalised (peakHoldDb) * barW;

        g.setColour (crimsonGlow.withAlpha (0.32f));
        g.fillRect (peakX - 1.6f, leftTrack.getY() - 1.0f, 3.2f,
                    rightTrack.getBottom() - leftTrack.getY() + 2.0f);
        g.setColour (crimsonBright.withAlpha (0.95f));
        g.fillRect (peakX - 0.6f, leftTrack.getY(), 1.2f,
                    rightTrack.getBottom() - leftTrack.getY());
    }

    // Bottom dB scale ticks and labels.
    constexpr float tickDbs[] { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, 0.0f };

    g.setFont (font (6.2f, FontWeight::regular, 0.02f));

    for (float db : tickDbs)
    {
        const float x = barLeft + dbToNormalised (db) * barW;

        g.setColour (obsidianGrid.brighter (0.15f));
        g.fillRect (x - 0.5f, rightTrack.getBottom() + 1.0f, 1.0f, 2.5f);

        g.setColour (textMuted.withAlpha (0.82f));
        g.drawText (juce::String (static_cast<int> (db)),
                    juce::Rectangle<float> (x - 8.0f, scaleRow.getY(), 16.0f, scaleRow.getHeight()),
                    juce::Justification::centred, false);
    }

    // Small screen sheen across the recessed well.
    juce::Graphics::ScopedSaveState state (g);
    juce::Path clip;
    clip.addRoundedRectangle (bounds, corner);
    g.reduceClipRegion (clip);
    g.setColour (juce::Colours::white.withAlpha (0.022f));
    g.fillRect (bounds.withHeight (bounds.getHeight() * 0.34f));
}

//==============================================================================
TactileButton::TactileButton (juce::AudioProcessorValueTreeState& state,
                              const juce::String& parameterID,
                              juce::String buttonLabel)
    : juce::Button (buttonLabel),
      parameter (state.getParameter (parameterID)),
      label (std::move (buttonLabel))
{
    jassert (parameter != nullptr);

    setTooltip (label);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (true);
    setButtonText (label);

    attachment = std::make_unique<juce::ParameterAttachment> (*parameter, [this] (float value)
    {
        setOn (value > 0.5f);
    });

    attachment->sendInitialUpdate();
}

void TactileButton::clicked()
{
    setOn (! on);
    attachment->setValueAsCompleteGesture (on ? 1.0f : 0.0f);
}

void TactileButton::setOn (bool shouldBeOn)
{
    if (on == shouldBeOn)
        return;

    on = shouldBeOn;
    repaint();
}

void TactileButton::paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    constexpr float corner = 3.5f;

    g.setColour (obsidianWell);
    g.fillRoundedRectangle (bounds.expanded (1.0f), corner + 0.8f);

    juce::ColourGradient capGradient (on ? gunmetalLight.interpolatedWith (crimsonGlow, 0.14f)
                                         : gunmetalBase,
                                      bounds.getTopLeft(),
                                      gunmetalDeep,
                                      bounds.getBottomRight(),
                                      false);
    g.setGradientFill (capGradient);
    g.fillRoundedRectangle (bounds, corner);

    if (isButtonDown)
    {
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRoundedRectangle (bounds, corner);
    }

    if (on)
    {
        g.setColour (crimsonGlow.withAlpha (0.34f));
        g.drawRoundedRectangle (bounds.expanded (1.0f), corner + 0.5f, 2.8f);

        g.setColour (crimsonAccent);
        g.drawRoundedRectangle (bounds, corner, 1.2f);

        g.setColour (crimsonBright.withAlpha (0.72f));
        g.drawRoundedRectangle (bounds.reduced (0.4f), corner, 0.6f);

        g.setColour (crimsonGlow.withAlpha (0.10f));
        g.fillRoundedRectangle (bounds.reduced (1.2f), corner - 0.6f);
    }
    else
    {
        g.setColour (isMouseOverButton ? chamferLight : gunmetalLight.withAlpha (0.55f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    const auto dotCentre = juce::Point<float> (bounds.getX() + 11.0f, bounds.getCentreY());

    if (on)
    {
        drawCrimsonDot (g, dotCentre, 1.7f);
    }
    else
    {
        g.setColour (textMuted.withAlpha (0.55f));
        g.fillEllipse (dotCentre.x - 1.4f, dotCentre.y - 1.4f, 2.8f, 2.8f);
    }

    g.setFont (font (8.6f, on ? FontWeight::semibold : FontWeight::regular, 0.13f));
    g.setColour (on ? textPrimary : textSecondary);
    g.drawText (label.toUpperCase(),
                bounds.withTrimmedLeft (18.0f).withTrimmedRight (5.0f),
                juce::Justification::centred, false);
}

//==============================================================================
ModeSelector::ModeSelector (juce::AudioProcessorValueTreeState& state,
                            const juce::String& parameterID)
    : parameter (state.getParameter (parameterID))
{
    jassert (parameter != nullptr);

    for (const auto* name : geverb::params::modeNames)
        choices.add (juce::String (name).toUpperCase());

    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    attachment = std::make_unique<juce::ParameterAttachment> (*parameter, [this] (float value)
    {
        setSelected (juce::roundToInt (value));
    });

    attachment->sendInitialUpdate();
}

juce::Rectangle<float> ModeSelector::segmentBounds (int index) const
{
    const auto area = getLocalBounds().toFloat();
    const float gap = 4.0f;
    const float width = (area.getWidth() - gap * 2.0f) / 3.0f;
    return { area.getX() + static_cast<float> (index) * (width + gap), area.getY(), width, area.getHeight() };
}

int ModeSelector::indexAt (juce::Point<float> position) const
{
    for (int i = 0; i < 3; ++i)
        if (segmentBounds (i).contains (position))
            return i;

    return -1;
}

void ModeSelector::setSelected (int newIndex)
{
    const int clamped = juce::jlimit (0, 2, newIndex);

    if (selected == clamped)
        return;

    selected = clamped;
    repaint();
}

void ModeSelector::mouseDown (const juce::MouseEvent& event)
{
    const int index = indexAt (event.position);

    if (index >= 0)
        attachment->setValueAsCompleteGesture (static_cast<float> (index));
}

void ModeSelector::mouseMove (const juce::MouseEvent& event)
{
    const int index = indexAt (event.position);

    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void ModeSelector::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void ModeSelector::paint (juce::Graphics& g)
{
    for (int i = 0; i < choices.size(); ++i)
    {
        const auto segment = segmentBounds (i);
        const bool active = i == selected;
        const bool isHovered = i == hovered;

        g.setColour (obsidianWell);
        g.fillRoundedRectangle (segment.expanded (0.8f), 3.2f);

        g.setColour (active ? gunmetalLight.interpolatedWith (crimsonGlow, 0.13f)
                            : (isHovered ? gunmetalBase.brighter (0.10f) : gunmetalBase));
        g.fillRoundedRectangle (segment, 2.4f);

        if (active)
        {
            g.setColour (crimsonGlow.withAlpha (0.32f));
            g.drawRoundedRectangle (segment.expanded (0.8f), 3.0f, 2.4f);
            g.setColour (crimsonAccent);
            g.drawRoundedRectangle (segment, 2.4f, 1.1f);
        }
        else
        {
            g.setColour (isHovered ? chamferLight : gunmetalLight.withAlpha (0.52f));
            g.drawRoundedRectangle (segment, 2.4f, 0.8f);
        }

        g.setFont (font (8.4f, active ? FontWeight::semibold : FontWeight::regular, 0.12f));
        g.setColour (active ? textPrimary : textSecondary);
        g.drawText (choices[i], segment, juce::Justification::centred, false);
    }
}

//==============================================================================
PresetPill::PresetPill (GeverbAudioProcessor& p)
    : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Use the chevrons to step through the factory atmospheres");
}

void PresetPill::refresh()
{
    const int preset = processor.getCurrentPresetIndex();
    const bool dirty = processor.isPresetDirty();

    if (preset != lastPreset || dirty != lastDirty)
    {
        lastPreset = preset;
        lastDirty = dirty;
        repaint();
    }
}

void PresetPill::stepPreset (int direction)
{
    int preset = processor.getCurrentPresetIndex() + direction;
    preset = (preset % geverb::numFactoryPresets + geverb::numFactoryPresets) % geverb::numFactoryPresets;
    processor.applyPreset (preset);
    refresh();
}

void PresetPill::mouseUp (const juce::MouseEvent& event)
{
    const float navWidth = static_cast<float> (getHeight()) * 1.35f;

    if (event.position.x <= navWidth)
        stepPreset (-1);
    else if (event.position.x >= static_cast<float> (getWidth()) - navWidth)
        stepPreset (1);
}

void PresetPill::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const float radius = bounds.getHeight() * 0.5f;
    const float navWidth = bounds.getHeight() * 1.30f;
    const auto prevArea = bounds.withWidth (navWidth);
    const auto nextArea = bounds.withLeft (bounds.getRight() - navWidth);

    // Diffused crimson rim light around the recessed screen.
    g.setColour (crimsonGlow.withAlpha (0.30f));
    g.drawRoundedRectangle (bounds.expanded (1.4f), radius + 1.4f, 2.2f);

    juce::ColourGradient screenGradient (obsidianScreen.brighter (0.035f), bounds.getTopLeft(),
                                         obsidianScreen.darker (0.18f), bounds.getBottomRight(), false);
    g.setGradientFill (screenGradient);
    g.fillRoundedRectangle (bounds, radius);

    g.setColour (crimsonAccent.withAlpha (0.54f));
    g.drawRoundedRectangle (bounds, radius, 1.0f);
    g.setColour (glassRimHighlight.withAlpha (0.20f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), radius - 1.0f, 0.75f);

    auto drawChevron = [&] (juce::Rectangle<float> area, bool pointsLeft)
    {
        const auto centre = area.getCentre();
        const float size = 4.2f;
        const float dir = pointsLeft ? -1.0f : 1.0f;
        juce::Path path;
        path.startNewSubPath (centre.x - dir * size * 0.5f, centre.y - size);
        path.lineTo (centre.x + dir * size * 0.6f, centre.y);
        path.lineTo (centre.x - dir * size * 0.5f, centre.y + size);

        g.setColour (textSecondary);
        g.strokePath (path, juce::PathStrokeType (1.45f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    };

    drawChevron (prevArea, true);
    drawChevron (nextArea, false);

    g.setColour (obsidianGrid);
    g.fillRect (prevArea.getRight(), bounds.getY() + 6.0f, 1.0f, bounds.getHeight() - 12.0f);
    g.fillRect (nextArea.getX(), bounds.getY() + 6.0f, 1.0f, bounds.getHeight() - 12.0f);

    const auto preset = geverb::factoryPresets()[static_cast<size_t> (
        juce::jlimit (0, geverb::numFactoryPresets - 1, processor.getCurrentPresetIndex()))];

    const auto* state = &processor.getValueTreeState();
    const int mode = juce::roundToInt (state->getRawParameterValue (geverb::params::mode)->load());
    const float decay = state->getRawParameterValue (geverb::params::decay)->load();
    const float mix = state->getRawParameterValue (geverb::params::mix)->load();

    const auto textArea = bounds.withTrimmedLeft (navWidth + 7.0f)
                                .withTrimmedRight (navWidth + 7.0f);

    auto titleArea = textArea.withTrimmedTop (5.0f).withHeight (12.0f);
    auto subtitleArea = textArea.withTrimmedTop (18.0f).withHeight (9.0f);

    juce::String title = juce::String (preset.name).toUpperCase();

    if (processor.isPresetDirty())
        title += "  *";

    g.setColour (textPrimary);
    g.setFont (font (10.2f, FontWeight::semibold, 0.16f));
    g.drawText ("GEVERB  //  " + title, titleArea, juce::Justification::centred, false);

    const juce::String subtitle = modeName (mode) + "   ·   " + formatSeconds (decay)
                                + "   ·   " + juce::String (juce::roundToInt (mix)) + "% WET";

    g.setColour (textMuted);
    g.setFont (font (6.4f, FontWeight::semibold, 0.14f));
    g.drawText (subtitle, subtitleArea, juce::Justification::centred, false);
}

} // namespace geverb::ui
