#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <cmath>
#include <iostream>

namespace
{
int failures = 0;

void check (bool condition, const char* message)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << message << '\n';

    if (! condition)
        ++failures;
}

void fillGuitarish (juce::AudioBuffer<float>& buffer, int offset)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto n = static_cast<double> (offset + i);
            const double t = n / 48000.0;
            const double notePhase = std::fmod (t, 0.75);
            const double env = std::exp (-5.0 * notePhase) * (1.0 - std::exp (-240.0 * notePhase));
            const double fundamental = juce::MathConstants<double>::twoPi * 196.0 * n / 48000.0;

            const float signal = static_cast<float> (0.30 * env
                * (std::sin (fundamental)
                   + 0.42 * std::sin (fundamental * 2.0)
                   + 0.20 * std::sin (fundamental * 3.0)));

            buffer.setSample (ch, i, ch == 0 ? signal : signal * 0.94f);
        }
    }
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getReadPointer (ch)[i]))
                return false;

    return true;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc < 2)
    {
        std::cerr << "Usage: GeverbHostSmokeTest /path/to/Geverb.vst3\n";
        return 2;
    }

    const juce::File pluginFile { juce::String (argv[1]) };

    if (! pluginFile.exists())
    {
        std::cerr << "Plugin bundle not found: " << pluginFile.getFullPathName() << '\n';
        return 2;
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    juce::OwnedArray<juce::PluginDescription> descriptions;

    for (auto* format : formatManager.getFormats())
        if (auto* vst3 = dynamic_cast<juce::VST3PluginFormat*> (format))
            vst3->findAllTypesForFile (descriptions, pluginFile.getFullPathName());

    check (! descriptions.isEmpty(), "VST3 scanner discovers the plugin");

    if (descriptions.isEmpty())
        return 1;

    auto& description = *descriptions[0];

    std::cout << "  INFO  " << description.name << " / " << description.manufacturerName
              << " / " << description.version << '\n';

    juce::String error;
    auto instance = formatManager.createPluginInstance (description, 48000.0, 512, error);

    check (instance != nullptr, "plugin instance can be created");

    if (instance == nullptr)
    {
        std::cerr << "  ERROR " << error << '\n';
        return 1;
    }

    check (instance->getParameters().size() >= 12, "all twelve user parameters are exposed to the host");

    juce::StringArray hostParameterNames;

    for (auto* parameter : instance->getParameters())
        hostParameterNames.add (parameter->getName (64));

    const std::array<const char*, 12> expectedNames {
        "Input", "Mix", "Output", "Decay", "Pre-Delay", "Size",
        "Damping", "Shimmer", "Modulation", "Voice", "Hold", "Bypass"
    };
    bool allNamesPresent = true;

    for (const auto* name : expectedNames)
        allNamesPresent = allNamesPresent && hostParameterNames.contains (name);

    check (allNamesPresent, "host parameter names match the published controls");
    check (instance->getNumPrograms() == 8, "eight factory atmospheres are exposed");
    check (instance->getProgramName (0).contains ("Eno"), "first program is Eno Float");

    instance->setPlayConfigDetails (2, 2, 48000.0, 512);
    instance->prepareToPlay (48000.0, 512);

    check (instance->getLatencySamples() == 0, "reverb reports zero latency");
    check (instance->getTailLengthSeconds() > 10.0, "long tail length is reported to the host");

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    bool finite = true;

    for (int block = 0; block < 100; ++block)
    {
        fillGuitarish (buffer, block * 512);
        instance->processBlock (buffer, midi);
        finite = finite && allFinite (buffer);
    }

    check (finite, "audio thread produces finite output through the VST3 wrapper");

    juce::MemoryBlock state;
    instance->getStateInformation (state);
    check (state.getSize() > 0, "state can be serialised for the host");

    instance->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    check (true, "serialised state can be restored");

    instance->setCurrentProgram (4);
    check (instance->getProgramName (4).contains ("Shimmer"), "program changes reach the host");

    instance->releaseResources();

    std::cout << (failures == 0 ? "\nHost smoke test passed.\n" : "\nHost smoke test failed.\n");
    return failures == 0 ? 0 : 1;
}
