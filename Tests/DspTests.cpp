#include "Parameters.h"
#include "Presets.h"
#include "PluginProcessor.h"
#include "dsp/ReverbEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const std::string& message)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << message << '\n';

    if (! condition)
        ++failures;
}

float rms (const std::vector<float>& samples)
{
    if (samples.empty())
        return 0.0f;

    double sum = 0.0;

    for (float sample : samples)
        sum += static_cast<double> (sample) * sample;

    return static_cast<float> (std::sqrt (sum / static_cast<double> (samples.size())));
}

float peak (const std::vector<float>& samples)
{
    float result = 0.0f;

    for (float sample : samples)
        result = std::max (result, std::abs (sample));

    return result;
}

bool finite (const std::vector<float>& samples)
{
    for (float sample : samples)
        if (! std::isfinite (sample))
            return false;

    return true;
}

void setFloatParameter (GeverbAudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.getValueTreeState().getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void testImpulseAndTail()
{
    std::cout << "\nLong-tail engine\n";

    constexpr double sampleRate = 48000.0;
    constexpr int samples = 48000 * 3;

    geverb::dsp::ReverbEngine engine;
    engine.prepare (sampleRate);

    geverb::dsp::ReverbEngine::Parameters params;
    params.decaySeconds = 20.0f;
    params.predelayMs = 30.0f;
    params.size = 0.82f;
    params.damping = 0.42f;
    params.shimmer = 0.20f;
    params.modulation = 0.65f;
    params.mode = 2;
    engine.setParameters (params);
    engine.reset();

    std::vector<float> left;
    left.reserve (samples);
    std::vector<float> right;
    right.reserve (samples);

    for (int i = 0; i < samples; ++i)
    {
        const float input = i == 0 ? 1.0f : 0.0f;
        float wetL = 0.0f;
        float wetR = 0.0f;
        engine.processSample (input, input, wetL, wetR);
        left.push_back (wetL);
        right.push_back (wetR);
    }

    check (finite (left) && finite (right), "three seconds of impulse response remain finite");
    check (peak (left) > 1.0e-4f, "impulse produces an audible reverb return");

    const std::vector<float> early (left.begin() + 4800, left.begin() + 12000);
    const std::vector<float> late (left.begin() + 96000, left.begin() + 120000);
    check (rms (late) > 0.0f, "tail is still present after two seconds");
    check (rms (late) < rms (early), "tail decays over time");

    float correlation = 0.0f;
    float energyL = 0.0f;
    float energyR = 0.0f;

    for (int i = 0; i < samples; ++i)
    {
        correlation += left[static_cast<size_t> (i)] * right[static_cast<size_t> (i)];
        energyL += left[static_cast<size_t> (i)] * left[static_cast<size_t> (i)];
        energyR += right[static_cast<size_t> (i)] * right[static_cast<size_t> (i)];
    }

    correlation /= std::sqrt (std::max (1.0e-12f, energyL * energyR));
    check (std::abs (correlation) < 0.98f, "centred input produces a decorrelated stereo tail");
}

void testDecayLengths()
{
    std::cout << "\nDecay control\n";

    auto measureTail = [] (float decaySeconds)
    {
        geverb::dsp::ReverbEngine engine;
        engine.prepare (48000.0);

        geverb::dsp::ReverbEngine::Parameters params;
        params.decaySeconds = decaySeconds;
        params.predelayMs = 0.0f;
        params.size = 0.7f;
        params.damping = 0.2f;
        params.shimmer = 0.0f;
        params.modulation = 0.0f;
        params.mode = 1;
        engine.setParameters (params);
        engine.reset();

        double energy = 0.0;
        int count = 0;

        for (int i = 0; i < 48000 * 2; ++i)
        {
            float wetL = 0.0f;
            float wetR = 0.0f;
            engine.processSample (i == 0 ? 0.8f : 0.0f, i == 0 ? 0.8f : 0.0f, wetL, wetR);

            if (i >= 48000 && i < 72000)
            {
                energy += static_cast<double> (wetL) * wetL;
                ++count;
            }
        }

        return static_cast<float> (std::sqrt (energy / std::max (1, count)));
    };

    const float shortTail = measureTail (1.5f);
    const float longTail = measureTail (24.0f);
    check (longTail > shortTail * 2.0f, "longer decay produces a much larger late tail");

    geverb::dsp::ReverbEngine engine;
    engine.prepare (48000.0);

    geverb::dsp::ReverbEngine::Parameters params;
    params.decaySeconds = 1.0f;
    params.predelayMs = 0.0f;
    params.size = 0.5f;
    params.damping = 0.5f;
    params.shimmer = 0.0f;
    params.modulation = 0.0f;
    params.mode = 0;
    engine.setParameters (params);
    engine.reset();

    std::vector<float> tail;

    for (int i = 0; i < 48000 * 7; ++i)
    {
        float wetL = 0.0f;
        float wetR = 0.0f;
        engine.processSample (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, wetL, wetR);

        if (i >= 48000 * 4)
            tail.push_back (wetL);
    }

    check (rms (tail) < 2.0e-5f, "short decay returns to silence");
}

void testModesAndExtremes()
{
    std::cout << "\nModes and extreme settings\n";

    for (int mode = 0; mode < 3; ++mode)
    {
        geverb::dsp::ReverbEngine engine;
        engine.prepare (48000.0);

        geverb::dsp::ReverbEngine::Parameters params;
        params.decaySeconds = mode == 2 ? 30.0f : 0.5f;
        params.predelayMs = mode == 1 ? 300.0f : 0.0f;
        params.size = mode == 0 ? 0.0f : 1.0f;
        params.damping = mode == 1 ? 0.0f : 1.0f;
        params.shimmer = mode == 2 ? 1.0f : 0.0f;
        params.modulation = 1.0f;
        params.mode = mode;
        engine.setParameters (params);
        engine.reset();

        std::vector<float> output;

        for (int i = 0; i < 48000; ++i)
        {
            const float input = i % 1000 == 0 ? 0.8f : 0.0f;
            float wetL = 0.0f;
            float wetR = 0.0f;
            engine.processSample (input, input * 0.8f, wetL, wetR);
            output.push_back (wetL);
            output.push_back (wetR);
        }

        check (finite (output), "mode " + std::to_string (mode) + " remains stable at extreme settings");
        check (peak (output) < 20.0f, "mode " + std::to_string (mode) + " stays bounded");
    }
}

void testFreezeAndShimmer()
{
    std::cout << "\nHold and shimmer\n";

    geverb::dsp::ReverbEngine engine;
    engine.prepare (48000.0);

    geverb::dsp::ReverbEngine::Parameters params;
    params.decaySeconds = 12.0f;
    params.predelayMs = 20.0f;
    params.size = 0.8f;
    params.damping = 0.4f;
    params.shimmer = 0.0f;
    params.modulation = 0.5f;
    params.mode = 2;
    engine.setParameters (params);
    engine.reset();

    std::vector<float> before;
    std::vector<float> held;

    for (int i = 0; i < 48000; ++i)
    {
        float wetL = 0.0f;
        float wetR = 0.0f;
        engine.processSample (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, wetL, wetR);

        if (i > 24000)
            before.push_back (wetL);
    }

    params.freeze = true;
    engine.setParameters (params);

    for (int i = 0; i < 96000; ++i)
    {
        float wetL = 0.0f;
        float wetR = 0.0f;
        engine.processSample (0.0f, 0.0f, wetL, wetR);

        if (i > 48000)
            held.push_back (wetL);
    }

    check (finite (held), "hold keeps the reverb finite");
    check (rms (held) > rms (before) * 0.05f, "hold sustains the existing tail");

    geverb::dsp::ReverbEngine shimmer;
    shimmer.prepare (48000.0);

    params.freeze = false;
    params.shimmer = 0.85f;
    params.mode = 2;
    shimmer.setParameters (params);
    shimmer.reset();

    std::vector<float> withShimmer;
    std::vector<float> withoutShimmer;

    for (int i = 0; i < 48000 * 2; ++i)
    {
        const float input = 0.25f * std::sin (juce::MathConstants<float>::twoPi * 220.0f
                                              * static_cast<float> (i) / 48000.0f);
        float wetL = 0.0f;
        float wetR = 0.0f;
        shimmer.processSample (input, input, wetL, wetR);

        if (i > 48000)
            withShimmer.push_back (wetL);
    }

    geverb::dsp::ReverbEngine dryShimmer;
    dryShimmer.prepare (48000.0);
    params.shimmer = 0.0f;
    dryShimmer.setParameters (params);
    dryShimmer.reset();

    for (int i = 0; i < 48000 * 2; ++i)
    {
        const float input = 0.25f * std::sin (juce::MathConstants<float>::twoPi * 220.0f
                                              * static_cast<float> (i) / 48000.0f);
        float wetL = 0.0f;
        float wetR = 0.0f;
        dryShimmer.processSample (input, input, wetL, wetR);

        if (i > 48000)
            withoutShimmer.push_back (wetL);
    }

    float difference = 0.0f;

    for (size_t i = 0; i < std::min (withShimmer.size(), withoutShimmer.size()); ++i)
    {
        const float delta = withShimmer[i] - withoutShimmer[i];
        difference += delta * delta;
    }

    check (std::sqrt (difference / static_cast<float> (withShimmer.size())) > 1.0e-4f,
           "shimmer changes the reverb return");
}

void testProcessorDryAndBypass()
{
    std::cout << "\nProcessor dry path\n";

    GeverbAudioProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    setFloatParameter (processor, geverb::params::mix, 0.0f);
    setFloatParameter (processor, geverb::params::input, 0.0f);
    setFloatParameter (processor, geverb::params::output, 0.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::AudioBuffer<float> original (2, 512);
    juce::MidiBuffer midi;

    for (int block = 0; block < 80; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < 512; ++i)
            {
                const int n = block * 512 + i;
                const float value = 0.25f * std::sin (juce::MathConstants<float>::twoPi * 196.0f
                                                      * static_cast<float> (n) / 48000.0f);
                buffer.setSample (ch, i, value);
                original.setSample (ch, i, value);
            }
        }

        processor.processBlock (buffer, midi);
    }

    float maxDryError = 0.0f;

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            maxDryError = std::max (maxDryError, std::abs (buffer.getSample (ch, i)
                                                           - original.getSample (ch, i)));

    check (maxDryError < 1.0e-5f, "mix at zero returns the dry signal");

    setFloatParameter (processor, geverb::params::bypass, 1.0f);

    for (int block = 0; block < 80; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
            {
                const int n = block * 512 + i;
                const float value = 0.25f * std::sin (juce::MathConstants<float>::twoPi * 196.0f
                                                      * static_cast<float> (n) / 48000.0f);
                buffer.setSample (ch, i, value);
                original.setSample (ch, i, value);
            }

        processor.processBlock (buffer, midi);
    }

    float maxBypassError = 0.0f;

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            maxBypassError = std::max (maxBypassError, std::abs (buffer.getSample (ch, i)
                                                                 - original.getSample (ch, i)));

    check (maxBypassError < 1.0e-5f, "bypass passes the host signal sample-accurately");
    check (processor.getLatencySamples() == 0, "reverb reports zero latency");
}

void testProcessorPresetsAndState()
{
    std::cout << "\nPresets and state\n";

    GeverbAudioProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    check (processor.getNumPrograms() == geverb::numFactoryPresets, "eight factory programs are exposed");
    check (processor.getParameters().size() == 12, "twelve host parameters are exposed");
    check (processor.getTailLengthSeconds() > 10.0, "a long host tail length is reported");

    processor.applyPreset (1);
    check (std::abs (processor.getValueTreeState().getRawParameterValue (geverb::params::decay)->load() - 28.0f) < 0.01f,
           "preset recall sets the decay time");
    check (juce::roundToInt (processor.getValueTreeState().getRawParameterValue (geverb::params::mode)->load()) == 2,
           "preset recall sets the voice");
    check (processor.getCurrentProgram() == 1, "current program tracks preset recall");
    check (! processor.isPresetDirty(), "preset recall clears the dirty flag");

    setFloatParameter (processor, geverb::params::size, 66.0f);
    check (processor.isPresetDirty(), "manual edits mark the preset dirty");

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    check (state.getSize() > 0, "plugin state serialises");

    setFloatParameter (processor, geverb::params::decay, 2.0f);
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    check (std::abs (processor.getValueTreeState().getRawParameterValue (geverb::params::decay)->load() - 28.0f) < 0.01f,
           "plugin state restores the decay time");
    check (std::abs (processor.getValueTreeState().getRawParameterValue (geverb::params::size)->load() - 66.0f) < 0.01f,
           "plugin state restores edited parameters");
    check (processor.getCurrentProgram() == 1, "plugin state restores the preset readout");
}

void testWaveformTelemetry()
{
    std::cout << "\nWaveform telemetry\n";

    GeverbAudioProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    setFloatParameter (processor, geverb::params::mix, 100.0f);
    processor.getValueTreeState().getParameter (geverb::params::bypass)->setValueNotifyingHost (0.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    for (int block = 0; block < 30; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.4f * std::sin (juce::MathConstants<float>::twoPi * 330.0f
                                                          * static_cast<float> (block * 512 + i) / 48000.0f));

        processor.processBlock (buffer, midi);
    }

    check (processor.getWaveformWriteIndex() > 10, "waveform ring advances");
    check (processor.getInputLevel() > 0.1f, "input meter reports signal");
    check (processor.getOutputLevel() > 0.0f, "output meter reports signal");
    check (processor.getOutputLevelLeft() > 0.0f, "left output meter reports signal");
    check (processor.getOutputLevelRight() > 0.0f, "right output meter reports signal");
    check (processor.getTailEnergy() > 0.0f, "tail energy is exposed to the display");
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::cout << "Geverb DSP / integration tests\n";

    testImpulseAndTail();
    testDecayLengths();
    testModesAndExtremes();
    testFreezeAndShimmer();
    testProcessorDryAndBypass();
    testProcessorPresetsAndState();
    testWaveformTelemetry();

    std::cout << (failures == 0 ? "\nAll tests passed.\n" : "\nTests failed.\n");
    return failures == 0 ? 0 : 1;
}
