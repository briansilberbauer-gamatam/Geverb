#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/ReverbEngine.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

class GeverbAudioProcessor final : public juce::AudioProcessor,
                                   private juce::AudioProcessorValueTreeState::Listener
{
public:
    GeverbAudioProcessor();
    ~GeverbAudioProcessor() override;

    using juce::AudioProcessor::processBlock;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Geverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 32.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept { return apvts; }

    int getCurrentPresetIndex() const noexcept { return currentPreset.load (std::memory_order_relaxed); }
    bool isPresetDirty() const noexcept { return presetDirty.load (std::memory_order_relaxed); }
    void setPresetIndexFromEditor (int index) noexcept { currentPreset.store (index, std::memory_order_relaxed); }

    float getInputLevel() const noexcept { return inputLevel.load (std::memory_order_relaxed); }
    float getOutputLevel() const noexcept { return outputLevel.load (std::memory_order_relaxed); }
    float getOutputLevelLeft() const noexcept { return outputLevelLeft.load (std::memory_order_relaxed); }
    float getOutputLevelRight() const noexcept { return outputLevelRight.load (std::memory_order_relaxed); }
    float getTailEnergy() const noexcept { return tailEnergy.load (std::memory_order_relaxed); }

    void applyPreset (int index);

    // Lock-free, decimated telemetry for the central waveform display.
    static constexpr int waveformSize = 1024;
    static constexpr int waveformDecimation = 64;
    static constexpr int waveformMask = waveformSize - 1;

    uint32_t getWaveformWriteIndex() const noexcept
    {
        return waveformWriteIndex.load (std::memory_order_relaxed);
    }

    float getInputWaveformSample (uint32_t index) const noexcept
    {
        return inputWaveformRing[index & waveformMask].load (std::memory_order_relaxed);
    }

    float getWetWaveformSample (uint32_t index) const noexcept
    {
        return wetWaveformRing[index & waveformMask].load (std::memory_order_relaxed);
    }

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    geverb::dsp::ReverbEngine engine;

    juce::SmoothedValue<float> inputGainSmoothed;
    juce::SmoothedValue<float> outputGainSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> bypassSmoothed;

    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* predelayParam = nullptr;
    std::atomic<float>* sizeParam = nullptr;
    std::atomic<float>* dampingParam = nullptr;
    std::atomic<float>* shimmerParam = nullptr;
    std::atomic<float>* modulationParam = nullptr;
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    std::atomic<int> currentPreset { 0 };
    std::atomic<bool> presetDirty { false };
    std::atomic<bool> applyingPreset { false };

    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> outputLevelLeft { 0.0f };
    std::atomic<float> outputLevelRight { 0.0f };
    std::atomic<float> tailEnergy { 0.0f };

    std::array<std::atomic<float>, waveformSize> inputWaveformRing {};
    std::array<std::atomic<float>, waveformSize> wetWaveformRing {};
    std::atomic<uint32_t> waveformWriteIndex { 0 };

    double currentSampleRate = 48000.0;
    int preparedChannels = 2;
    int waveformAccumulatorCount = 0;
    float waveformInputAccumulator = 0.0f;
    float waveformWetAccumulator = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeverbAudioProcessor)
};
