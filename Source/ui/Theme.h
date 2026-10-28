#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace geverb::ui
{
    // --- Dark Studio Surface & Ambient Soft-Box Backdrop ---
    inline const juce::Colour studioSurfaceLight { 0xff12151c };
    inline const juce::Colour studioSurfaceDark  { 0xff07080b };
    inline const juce::Colour dropShadowDeep     { 0xff020305 };

    // --- Brushed Gunmetal Base Chassis & Chamfered Edges ---
    inline const juce::Colour gunmetalLight      { 0xff2b3038 };
    inline const juce::Colour gunmetalBase       { 0xff1d2026 };
    inline const juce::Colour gunmetalDeep       { 0xff13151a };
    inline const juce::Colour chamferLight       { 0xff343a44 };
    inline const juce::Colour chamferDark        { 0xff0e1014 };

    // --- Obsidian Black Screens & Recessed Cavities ---
    inline const juce::Colour obsidianScreen     { 0xff090a0d };
    inline const juce::Colour obsidianWell       { 0xff0d0f13 };
    inline const juce::Colour obsidianGrid       { 0xff181b22 };

    // --- Transparent Glass Overlay & Soft-Box Reflections ---
    inline const juce::Colour glassTintTop       { 0x1affffff };
    inline const juce::Colour glassTintBottom    { 0x26050608 };
    inline const juce::Colour glassSpecular      { 0x14ffffff };
    inline const juce::Colour glassRimHighlight  { 0x33ffffff };

    // --- Muted, Sophisticated Crimson Red Accents ---
    inline const juce::Colour crimsonAccent      { 0xffc82230 };
    inline const juce::Colour crimsonBright      { 0xffef3b49 };
    inline const juce::Colour crimsonGlow        { 0xff8c141f };
    inline const juce::Colour crimsonSubtle      { 0xff3d1218 };

    // --- Clean Minimalist Sans-Serif Typography ---
    inline const juce::Colour textPrimary        { 0xffeceff4 };
    inline const juce::Colour textSecondary      { 0xff929aa8 };
    inline const juce::Colour textMuted          { 0xff545b68 };

    enum class FontWeight
    {
        regular,
        semibold,
        black
    };

    juce::Font font (float height, FontWeight weight = FontWeight::regular, float tracking = 0.0f);

    juce::String formatDb (float value);
    juce::String formatPercent (float value);
    juce::String formatSeconds (float value);
    juce::String formatMs (float value);
    juce::String formatMode (int mode);
    juce::String formatSize (float value);

    // Dark studio surface, dimensional chamfered gunmetal unit, brushed grain,
    // flush screwless glass overlay and the crimson LED edge-glow.
    void drawDesktopHardwareChassis (juce::Graphics&, juce::Rectangle<float> bounds);

    // Engraved section label with a tiny crimson index tick and muted subtitle.
    void drawEngravedLabel (juce::Graphics&, juce::Rectangle<float> bounds,
                            const juce::String& title, const juce::String& subtitle);

    // Recessed obsidian glass well used behind screens and readouts.
    void drawGlassWell (juce::Graphics&, juce::Rectangle<float> bounds, float corner);

    // Small crimson status bead with a diffused halo.
    void drawCrimsonDot (juce::Graphics&, juce::Point<float> centre, float radius);

} // namespace geverb::ui
