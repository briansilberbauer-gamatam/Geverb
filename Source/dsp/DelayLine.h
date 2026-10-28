#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace geverb::dsp
{
/** A circular delay line with Catmull-Rom fractional reads. */
class FractionalDelay
{
public:
    void prepare (int maxDelaySamples)
    {
        const int requested = std::max (8, maxDelaySamples + 8);
        bufferSize = 1;

        while (bufferSize < requested)
            bufferSize <<= 1;

        buffer.assign (static_cast<size_t> (bufferSize), 0.0f);
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    void write (float sample) noexcept
    {
        buffer[static_cast<size_t> (writeIndex)] = sample;
        writeIndex = (writeIndex + 1) & (bufferSize - 1);
    }

    float read (float delaySamples) const noexcept
    {
        if (bufferSize == 0)
            return 0.0f;

        const float maxDelay = static_cast<float> (bufferSize - 4);
        const float delay = std::clamp (delaySamples, 0.0f, maxDelay);

        float position = static_cast<float> (writeIndex) - delay;
        while (position < 0.0f)
            position += static_cast<float> (bufferSize);
        while (position >= static_cast<float> (bufferSize))
            position -= static_cast<float> (bufferSize);

        const int i1 = static_cast<int> (position);
        const float fraction = position - static_cast<float> (i1);
        const int i0 = (i1 - 1) & (bufferSize - 1);
        const int i2 = (i1 + 1) & (bufferSize - 1);
        const int i3 = (i1 + 2) & (bufferSize - 1);

        const float y0 = buffer[static_cast<size_t> (i0)];
        const float y1 = buffer[static_cast<size_t> (i1)];
        const float y2 = buffer[static_cast<size_t> (i2)];
        const float y3 = buffer[static_cast<size_t> (i3)];

        // Catmull-Rom interpolation keeps the long, slowly modulated tails smooth.
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * fraction + c2) * fraction + c1) * fraction + c0;
    }

    bool isPrepared() const noexcept { return bufferSize > 0; }

private:
    std::vector<float> buffer;
    int bufferSize = 0;
    int writeIndex = 0;
};

/** Schroeder all-pass used for the input diffusion stage. */
class Allpass
{
public:
    void prepare (int delaySamples)
    {
        delay = std::max (1, delaySamples);
        buffer.assign (static_cast<size_t> (delay), 0.0f);
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    void setFeedback (float newFeedback) noexcept { feedback = newFeedback; }

    float process (float input) noexcept
    {
        const float delayed = buffer[static_cast<size_t> (writeIndex)];
        const float output = delayed - feedback * input;
        buffer[static_cast<size_t> (writeIndex)] = input + feedback * output;

        if (++writeIndex >= delay)
            writeIndex = 0;

        return output;
    }

private:
    std::vector<float> buffer;
    int delay = 1;
    int writeIndex = 0;
    float feedback = 0.55f;
};

/** One-pole low-pass with denormal-safe state. */
class OnePoleLowpass
{
public:
    void setCutoff (float cutoffHz, double sampleRate) noexcept
    {
        const float nyquist = static_cast<float> (sampleRate * 0.5);
        const float cutoff = std::clamp (cutoffHz, 10.0f, nyquist * 0.98f);
        coefficient = 1.0f - std::exp (-juceTwoPi * cutoff / static_cast<float> (sampleRate));
        coefficient = std::clamp (coefficient, 0.00001f, 1.0f);
    }

    float process (float input) noexcept
    {
        state += coefficient * (input - state);
        state = denormalGuard (state);
        return state;
    }

    void reset() noexcept { state = 0.0f; }

private:
    static float denormalGuard (float value) noexcept
    {
        return std::abs (value) < 1.0e-24f ? 0.0f : value;
    }

    static constexpr float juceTwoPi = 6.28318530717958647692f;
    float coefficient = 1.0f;
    float state = 0.0f;
};

/** One-pole high-pass (input minus a low-pass state). */
class OnePoleHighpass
{
public:
    void setCutoff (float cutoffHz, double sampleRate) noexcept
    {
        lowpass.setCutoff (cutoffHz, sampleRate);
    }

    float process (float input) noexcept
    {
        return input - lowpass.process (input);
    }

    void reset() noexcept { lowpass.reset(); }

private:
    OnePoleLowpass lowpass;
};

/**
    A two-grain, Hann-windowed octave-up shifter. Two overlapping grains are
    read at twice the write speed and crossfaded with sin^2 windows, which sum
    to unity so the shimmer bed has no periodic amplitude modulation.
*/
class OctaveUp
{
public:
    void prepare (double sampleRate, float grainLengthSeconds)
    {
        grainSamples = std::max (256.0f, static_cast<float> (sampleRate) * grainLengthSeconds);
        line.prepare (static_cast<int> (grainSamples * 2.2f) + 8);

        phases[0] = 0.0f;
        phases[1] = 0.5f;
        reset();
    }

    void reset() noexcept
    {
        line.reset();
        phases[0] = 0.0f;
        phases[1] = 0.5f;
    }

    float process (float input) noexcept
    {
        line.write (input);

        float output = 0.0f;
        const float increment = 1.0f / grainSamples;

        for (int grain = 0; grain < 2; ++grain)
        {
            const float phase = phases[grain];
            const float window = 0.5f - 0.5f * std::cos (6.28318530717958647692f * phase);
            const float delay = 2.0f * grainSamples * (1.0f - phase);
            output += line.read (delay) * window;

            phases[grain] += increment;

            if (phases[grain] >= 1.0f)
                phases[grain] -= 1.0f;
        }

        return output;
    }

private:
    FractionalDelay line;
    float grainSamples = 2048.0f;
    float phases[2] { 0.0f, 0.5f };
};

} // namespace geverb::dsp
