#include "PluginProcessor.h"

#include "Parameters.h"
#include "Presets.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
using APVTS = juce::AudioProcessorValueTreeState;

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id,
                                                      const char* name,
                                                      juce::NormalisableRange<float> range,
                                                      float defaultValue,
                                                      const char* label)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 },
        name,
        range,
        defaultValue,
        juce::AudioParameterFloatAttributes().withLabel (label));
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id,
                                                    const char* name,
                                                    bool defaultValue)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 },
                                                       name,
                                                       defaultValue);
}
} // namespace

GeverbAudioProcessor::GeverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    inputParam = apvts.getRawParameterValue (geverb::params::input);
    mixParam = apvts.getRawParameterValue (geverb::params::mix);
    outputParam = apvts.getRawParameterValue (geverb::params::output);
    decayParam = apvts.getRawParameterValue (geverb::params::decay);
    predelayParam = apvts.getRawParameterValue (geverb::params::predelay);
    sizeParam = apvts.getRawParameterValue (geverb::params::size);
    dampingParam = apvts.getRawParameterValue (geverb::params::damping);
    shimmerParam = apvts.getRawParameterValue (geverb::params::shimmer);
    modulationParam = apvts.getRawParameterValue (geverb::params::modulation);
    modeParam = apvts.getRawParameterValue (geverb::params::mode);
    holdParam = apvts.getRawParameterValue (geverb::params::hold);
    bypassParam = apvts.getRawParameterValue (geverb::params::bypass);

    for (const auto* parameterID : geverb::params::allParameterIDs)
        apvts.addParameterListener (parameterID, this);

    for (auto& sample : inputWaveformRing)
        sample.store (0.0f, std::memory_order_relaxed);

    for (auto& sample : wetWaveformRing)
        sample.store (0.0f, std::memory_order_relaxed);

    currentPreset.store (0, std::memory_order_relaxed);
    presetDirty.store (false, std::memory_order_relaxed);
}

GeverbAudioProcessor::~GeverbAudioProcessor()
{
    for (const auto* parameterID : geverb::params::allParameterIDs)
        apvts.removeParameterListener (parameterID, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout GeverbAudioProcessor::createParameterLayout()
{
    using namespace geverb::params;

    APVTS::ParameterLayout layout;

    auto mixer = std::make_unique<juce::AudioProcessorParameterGroup> ("mixer", "Mixer", " | ");
    mixer->addChild (makeFloat (input, "Input", { -24.0f, 24.0f, 0.01f }, inputDefault, "dB"));
    mixer->addChild (makeFloat (mix, "Mix", { 0.0f, 100.0f, 0.01f }, mixDefault, "%"));
    mixer->addChild (makeFloat (output, "Output", { -24.0f, 12.0f, 0.01f }, outputDefault, "dB"));
    layout.add (std::move (mixer));

    auto space = std::make_unique<juce::AudioProcessorParameterGroup> ("space", "Space", " | ");
    space->addChild (makeFloat (decay, "Decay", { 0.5f, 30.0f, 0.01f }, decayDefault, "s"));
    space->addChild (makeFloat (predelay, "Pre-Delay", { 0.0f, 300.0f, 0.01f }, predelayDefault, "ms"));
    space->addChild (makeFloat (size, "Size", { 0.0f, 100.0f, 0.01f }, sizeDefault, "%"));
    space->addChild (makeFloat (damping, "Damping", { 0.0f, 100.0f, 0.01f }, dampingDefault, "%"));
    space->addChild (makeFloat (shimmer, "Shimmer", { 0.0f, 100.0f, 0.01f }, shimmerDefault, "%"));
    space->addChild (makeFloat (modulation, "Modulation", { 0.0f, 100.0f, 0.01f }, modulationDefault, "%"));
    space->addChild (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { mode, 1 },
        "Voice",
        juce::StringArray { "Plate", "Hall", "Ambient" },
        modeDefault));
    layout.add (std::move (space));

    auto utility = std::make_unique<juce::AudioProcessorParameterGroup> ("utility", "Utility", " | ");
    utility->addChild (makeBool (hold, "Hold", false));
    utility->addChild (makeBool (bypass, "Bypass", false));
    layout.add (std::move (utility));

    return layout;
}

void GeverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = std::max (8000.0, sampleRate);
    preparedChannels = juce::jlimit (1, 2, juce::jmax (1, juce::jmax (getTotalNumInputChannels(),
                                                                      getTotalNumOutputChannels())));

    engine.prepare (currentSampleRate);

    inputGainSmoothed.reset (currentSampleRate, 0.02);
    outputGainSmoothed.reset (currentSampleRate, 0.02);
    mixSmoothed.reset (currentSampleRate, 0.02);
    bypassSmoothed.reset (currentSampleRate, 0.02);

    inputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputParam->load()));
    outputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    mixSmoothed.setCurrentAndTargetValue (mixParam->load() * 0.01f);
    bypassSmoothed.setCurrentAndTargetValue (bypassParam->load() > 0.5f ? 1.0f : 0.0f);

    geverb::dsp::ReverbEngine::Parameters params;
    params.decaySeconds = decayParam->load();
    params.predelayMs = predelayParam->load();
    params.size = sizeParam->load() * 0.01f;
    params.damping = dampingParam->load() * 0.01f;
    params.shimmer = shimmerParam->load() * 0.01f;
    params.modulation = modulationParam->load() * 0.01f;
    params.mode = juce::roundToInt (modeParam->load());
    params.freeze = holdParam->load() > 0.5f;
    engine.setParameters (params);

    inputLevel.store (0.0f, std::memory_order_relaxed);
    outputLevel.store (0.0f, std::memory_order_relaxed);
    outputLevelLeft.store (0.0f, std::memory_order_relaxed);
    outputLevelRight.store (0.0f, std::memory_order_relaxed);
    tailEnergy.store (0.0f, std::memory_order_relaxed);

    for (auto& sample : inputWaveformRing)
        sample.store (0.0f, std::memory_order_relaxed);

    for (auto& sample : wetWaveformRing)
        sample.store (0.0f, std::memory_order_relaxed);

    waveformWriteIndex.store (0, std::memory_order_relaxed);
    waveformAccumulatorCount = 0;
    waveformInputAccumulator = 0.0f;
    waveformWetAccumulator = 0.0f;

    setLatencySamples (0);
}

void GeverbAudioProcessor::releaseResources()
{
    engine.reset();
}

void GeverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 2; ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0 || numChannels <= 0)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) : left;

    geverb::dsp::ReverbEngine::Parameters params;
    params.decaySeconds = decayParam->load();
    params.predelayMs = predelayParam->load();
    params.size = sizeParam->load() * 0.01f;
    params.damping = dampingParam->load() * 0.01f;
    params.shimmer = shimmerParam->load() * 0.01f;
    params.modulation = modulationParam->load() * 0.01f;
    params.mode = juce::roundToInt (modeParam->load());
    params.freeze = holdParam->load() > 0.5f;
    engine.setParameters (params);

    inputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (inputParam->load()));
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, mixParam->load() * 0.01f));

    const bool bypassed = bypassParam->load() > 0.5f;
    bypassSmoothed.setTargetValue (bypassed ? 1.0f : 0.0f);

    float peakIn = 0.0f;
    float peakOut = 0.0f;
    float peakOutLeft = 0.0f;
    float peakOutRight = 0.0f;
    double wetEnergy = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = left[i];
        const float inR = numChannels > 1 ? right[i] : inL;

        const float inputGain = inputGainSmoothed.getNextValue();
        const float dryL = inL * inputGain;
        const float dryR = inR * inputGain;

        float wetL = 0.0f;
        float wetR = 0.0f;
        engine.processSample (dryL, dryR, wetL, wetR);

        const float mix = mixSmoothed.getNextValue();
        const float outputGain = outputGainSmoothed.getNextValue();
        const float bypass = bypassSmoothed.getNextValue();

        const float processedL = (dryL * (1.0f - mix) + wetL * mix) * outputGain;
        const float processedR = (dryR * (1.0f - mix) + wetR * mix) * outputGain;

        float outL = processedL * (1.0f - bypass) + inL * bypass;
        float outR = processedR * (1.0f - bypass) + inR * bypass;

        if (! std::isfinite (outL) || ! std::isfinite (outR))
        {
            outL = 0.0f;
            outR = 0.0f;
            engine.reset();
        }

        if (numChannels > 1)
        {
            left[i] = outL;
            right[i] = outR;
        }
        else
        {
            left[i] = 0.5f * (outL + outR);
        }

        peakIn = std::max (peakIn, std::max (std::abs (inL), std::abs (inR)));
        peakOutLeft = std::max (peakOutLeft, std::abs (outL));
        peakOutRight = std::max (peakOutRight, std::abs (outR));
        peakOut = std::max (peakOut, std::max (std::abs (outL), std::abs (outR)));
        wetEnergy += 0.5 * (static_cast<double> (wetL) * wetL + static_cast<double> (wetR) * wetR);

        waveformInputAccumulator += 0.5f * (inL + inR);
        waveformWetAccumulator += 0.5f * (wetL + wetR);

        if (++waveformAccumulatorCount >= waveformDecimation)
        {
            const auto write = waveformWriteIndex.load (std::memory_order_relaxed);
            inputWaveformRing[write & waveformMask].store (waveformInputAccumulator / waveformDecimation,
                                                           std::memory_order_relaxed);
            wetWaveformRing[write & waveformMask].store (waveformWetAccumulator / waveformDecimation,
                                                          std::memory_order_relaxed);
            waveformWriteIndex.store (write + 1, std::memory_order_relaxed);

            waveformAccumulatorCount = 0;
            waveformInputAccumulator = 0.0f;
            waveformWetAccumulator = 0.0f;
        }
    }

    inputLevel.store (peakIn, std::memory_order_relaxed);
    outputLevel.store (peakOut, std::memory_order_relaxed);
    outputLevelLeft.store (peakOutLeft, std::memory_order_relaxed);
    outputLevelRight.store (peakOutRight, std::memory_order_relaxed);
    tailEnergy.store (static_cast<float> (std::sqrt (wetEnergy / static_cast<double> (numSamples))),
                      std::memory_order_relaxed);
}

void GeverbAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (parameterID, newValue);

    if (! applyingPreset.load (std::memory_order_relaxed))
        presetDirty.store (true, std::memory_order_relaxed);
}

int GeverbAudioProcessor::getNumPrograms()
{
    return geverb::numFactoryPresets;
}

int GeverbAudioProcessor::getCurrentProgram()
{
    return currentPreset.load (std::memory_order_relaxed);
}

void GeverbAudioProcessor::setCurrentProgram (int index)
{
    applyPreset (index);
}

const juce::String GeverbAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < geverb::numFactoryPresets)
        return geverb::factoryPresets()[static_cast<size_t> (index)].name;

    return "Eno Float";
}

void GeverbAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void GeverbAudioProcessor::applyPreset (int index)
{
    if (index < 0 || index >= geverb::numFactoryPresets)
        return;

    const auto& preset = geverb::factoryPresets()[static_cast<size_t> (index)];
    applyingPreset.store (true, std::memory_order_relaxed);

    auto setFloat = [this] (const char* id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    setFloat (geverb::params::input, preset.inputDb);
    setFloat (geverb::params::mix, preset.mix);
    setFloat (geverb::params::output, preset.outputDb);
    setFloat (geverb::params::decay, preset.decaySeconds);
    setFloat (geverb::params::predelay, preset.predelayMs);
    setFloat (geverb::params::size, preset.size);
    setFloat (geverb::params::damping, preset.damping);
    setFloat (geverb::params::shimmer, preset.shimmer);
    setFloat (geverb::params::modulation, preset.modulation);
    setFloat (geverb::params::mode, static_cast<float> (preset.mode));
    setFloat (geverb::params::hold, preset.hold ? 1.0f : 0.0f);
    setFloat (geverb::params::bypass, 0.0f);

    applyingPreset.store (false, std::memory_order_relaxed);
    currentPreset.store (index, std::memory_order_relaxed);
    presetDirty.store (false, std::memory_order_relaxed);
}

void GeverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetIndex", currentPreset.load (std::memory_order_relaxed), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GeverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    if (! tree.isValid())
        return;

    const int presetIndex = static_cast<int> (tree.getProperty ("presetIndex", 0));
    applyingPreset.store (true, std::memory_order_relaxed);
    apvts.replaceState (tree);
    applyingPreset.store (false, std::memory_order_relaxed);

    currentPreset.store (juce::jlimit (0, geverb::numFactoryPresets - 1, presetIndex),
                         std::memory_order_relaxed);
    presetDirty.store (false, std::memory_order_relaxed);
}

juce::AudioProcessorParameter* GeverbAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (geverb::params::bypass);
}

bool GeverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput = layouts.getMainInputChannelSet();

    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainInput == mainOutput;
}

juce::AudioProcessorEditor* GeverbAudioProcessor::createEditor()
{
    return new GeverbAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GeverbAudioProcessor();
}
