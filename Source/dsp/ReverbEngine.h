#pragma once

#include "DelayLine.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstdint>

namespace geverb::dsp
{
/**
    A lush, long-decay feedback-delay-network reverb designed for lead guitar.

    The signal path is: pre-delay -> four-stage diffusion -> eight-line Hadamard
    FDN with per-line damping and slow decorrelated modulation -> stereo taps.
    A parallel two-grain octave-up shimmer loop is fed from the FDN output.
*/
class ReverbEngine
{
public:
    struct Parameters
    {
        float decaySeconds = 18.0f;
        float predelayMs = 45.0f;
        float size = 0.82f;
        float damping = 0.46f;
        float shimmer = 0.22f;
        float modulation = 0.72f;
        int mode = 2;
        bool freeze = false;
    };

    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 4;

    void prepare (double sampleRate);
    void reset();
    void setParameters (const Parameters&) noexcept;

    void processSample (float inputL, float inputR,
                        float& wetL, float& wetR) noexcept;

    bool isPrepared() const noexcept { return prepared; }

private:
    void updateControlRate();
    float randomBipolar() noexcept;
    static void hadamard (std::array<float, numLines>& values) noexcept;

    std::array<FractionalDelay, numLines> lines;
    std::array<OnePoleLowpass, numLines> dampingFilters;
    std::array<OnePoleHighpass, numLines> dcBlockers;
    std::array<float, numLines> baseDelaySamples {};
    std::array<float, numLines> delaySamples {};
    std::array<float, numLines> feedbackGain {};
    std::array<float, numLines> modulationTarget {};
    std::array<float, numLines> modulationSmooth {};

    std::array<Allpass, numDiffusers> diffusersL;
    std::array<Allpass, numDiffusers> diffusersR;

    FractionalDelay preDelayL;
    FractionalDelay preDelayR;

    OnePoleHighpass inputHighpassL;
    OnePoleHighpass inputHighpassR;
    OnePoleHighpass outputHighpassL;
    OnePoleHighpass outputHighpassR;

    OctaveUp octaveL;
    OctaveUp octaveR;

    FractionalDelay shimmerDelayL;
    FractionalDelay shimmerDelayR;
    OnePoleLowpass shimmerInputToneL;
    OnePoleLowpass shimmerInputToneR;
    OnePoleLowpass shimmerLoopToneL;
    OnePoleLowpass shimmerLoopToneR;
    OnePoleHighpass shimmerHighpassL;
    OnePoleHighpass shimmerHighpassR;

    juce::SmoothedValue<float> sizeSmoothed;
    juce::SmoothedValue<float> decaySmoothed;
    juce::SmoothedValue<float> predelaySmoothed;
    juce::SmoothedValue<float> dampingSmoothed;
    juce::SmoothedValue<float> shimmerSmoothed;
    juce::SmoothedValue<float> modulationSmoothed;
    juce::SmoothedValue<float> freezeSmoothed;
    juce::SmoothedValue<float> modeScaleSmoothed;
    juce::SmoothedValue<float> modeDampingSmoothed;

    double sampleRate = 48000.0;
    int maxDelaySamples = 1;
    int controlCounter = 0;
    int controlTicks = 0;

    float lastFdnL = 0.0f;
    float lastFdnR = 0.0f;
    float shimmerDelaySamplesL = 1.0f;
    float shimmerDelaySamplesR = 1.0f;

    int mode = 2;
    uint32_t randomState = 0x9e3779b9u;
    bool prepared = false;
};
} // namespace geverb::dsp
