#pragma once

#include <array>

namespace geverb
{
    struct FactoryPreset
    {
        const char* name;
        const char* character;
        float inputDb;
        float mix;
        float outputDb;
        float decaySeconds;
        float predelayMs;
        float size;
        float damping;
        float shimmer;
        float modulation;
        int mode;
        bool hold;
    };

    inline const std::array<FactoryPreset, 8>& factoryPresets()
    {
        static const std::array<FactoryPreset, 8> presets {{
            { "Eno Float",     "AMBIENT / ENDLESS DRIFT",     0.0f, 52.0f, 0.0f, 18.0f, 45.0f, 82.0f, 46.0f, 22.0f, 72.0f, 2, false },
            { "Infinite Sky",  "AMBIENT / HOLD THE HORIZON",  0.0f, 62.0f, -1.0f, 28.0f, 65.0f, 96.0f, 60.0f, 48.0f, 82.0f, 2, false },
            { "Cathedral Lead", "HALL / WIDE & DARK",         -1.0f, 38.0f, 0.5f, 10.5f, 28.0f, 78.0f, 40.0f, 0.0f, 55.0f, 1, false },
            { "Plate Sustain", "PLATE / CLOSE & LUMINOUS",     0.0f, 33.0f, 0.0f, 5.5f, 12.0f, 54.0f, 18.0f, 0.0f, 38.0f, 0, false },
            { "Shimmer Veil",  "OCTAVE HALO / SLOW RISE",      0.0f, 58.0f, -1.0f, 20.0f, 35.0f, 85.0f, 42.0f, 62.0f, 76.0f, 2, false },
            { "Slow Bloom",    "HALL / PRE-DELAYED BLOOM",      0.0f, 55.0f, -0.5f, 16.0f, 90.0f, 90.0f, 52.0f, 12.0f, 60.0f, 1, false },
            { "Close Lush",    "PLATE / INTIMATE SUSTAIN",     0.0f, 27.0f, 0.0f, 3.2f, 8.0f, 38.0f, 12.0f, 0.0f, 28.0f, 0, false },
            { "Lead Into Light", "HALL / SOFT-FOCUS LEAD",     -0.5f, 48.0f, 0.0f, 13.5f, 55.0f, 88.0f, 33.0f, 18.0f, 62.0f, 1, false }
        }};

        return presets;
    }

    inline constexpr int numFactoryPresets = 8;
} // namespace geverb
