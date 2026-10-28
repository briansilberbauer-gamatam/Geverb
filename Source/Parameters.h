#pragma once

#include <array>

namespace geverb::params
{
    inline constexpr const char* input       = "input";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* output      = "output";
    inline constexpr const char* decay       = "decay";
    inline constexpr const char* predelay    = "predelay";
    inline constexpr const char* size        = "size";
    inline constexpr const char* damping     = "damping";
    inline constexpr const char* shimmer     = "shimmer";
    inline constexpr const char* modulation  = "modulation";
    inline constexpr const char* mode        = "mode";
    inline constexpr const char* hold        = "hold";
    inline constexpr const char* bypass      = "bypass";

    inline constexpr std::array<const char*, 12> allParameterIDs {
        input, mix, output, decay, predelay, size, damping, shimmer, modulation, mode, hold, bypass
    };

    inline constexpr std::array<const char*, 3> modeNames { "Plate", "Hall", "Ambient" };

    inline constexpr float inputDefault     = 0.0f;
    inline constexpr float mixDefault       = 52.0f;
    inline constexpr float outputDefault    = 0.0f;
    inline constexpr float decayDefault     = 18.0f;
    inline constexpr float predelayDefault  = 45.0f;
    inline constexpr float sizeDefault      = 82.0f;
    inline constexpr float dampingDefault   = 46.0f;
    inline constexpr float shimmerDefault   = 22.0f;
    inline constexpr float modulationDefault = 72.0f;
    inline constexpr int   modeDefault      = 2;
} // namespace geverb::params
