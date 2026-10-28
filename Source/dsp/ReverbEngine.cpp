#include "ReverbEngine.h"

#include <algorithm>
#include <cmath>

namespace geverb::dsp
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 6.28318530717958647692f;
constexpr float ln1000 = 6.90775527898213705205f;
constexpr float hadamardScale = 0.35355339059327376220f;

constexpr float modeScaleTable[3] { 0.76f, 1.0f, 1.26f };
constexpr float modeDampingTable[3] { 1.06f, 0.88f, 0.64f };
constexpr float modeDiffusionTable[3] { 0.61f, 0.55f, 0.68f };

constexpr double baseDelaySeconds[ReverbEngine::numLines] {
    0.0311, 0.0367, 0.0419, 0.0473, 0.0541, 0.0617, 0.0683, 0.0739
};

constexpr double diffuserLengthsL[ReverbEngine::numDiffusers] { 0.0047, 0.0071, 0.0031, 0.0097 };
constexpr double diffuserLengthsR[ReverbEngine::numDiffusers] { 0.0059, 0.0083, 0.0037, 0.0113 };
} // namespace

void ReverbEngine::prepare (double newSampleRate)
{
    sampleRate = std::max (8000.0, newSampleRate);
    prepared = false;

    maxDelaySamples = static_cast<int> (sampleRate * 0.30) + 8;

    for (int i = 0; i < numLines; ++i)
    {
        lines[static_cast<size_t> (i)].prepare (maxDelaySamples);
        baseDelaySamples[static_cast<size_t> (i)] = static_cast<float> (baseDelaySeconds[i] * sampleRate);
        delaySamples[static_cast<size_t> (i)] = baseDelaySamples[static_cast<size_t> (i)];
        feedbackGain[static_cast<size_t> (i)] = 0.0f;
        modulationTarget[static_cast<size_t> (i)] = 0.0f;
        modulationSmooth[static_cast<size_t> (i)] = 0.0f;
    }

    for (int i = 0; i < numDiffusers; ++i)
    {
        diffusersL[static_cast<size_t> (i)].prepare (std::max (1, static_cast<int> (sampleRate * diffuserLengthsL[i])));
        diffusersR[static_cast<size_t> (i)].prepare (std::max (1, static_cast<int> (sampleRate * diffuserLengthsR[i])));
    }

    preDelayL.prepare (static_cast<int> (sampleRate * 0.45));
    preDelayR.prepare (static_cast<int> (sampleRate * 0.45));

    inputHighpassL.setCutoff (58.0f, sampleRate);
    inputHighpassR.setCutoff (58.0f, sampleRate);
    outputHighpassL.setCutoff (42.0f, sampleRate);
    outputHighpassR.setCutoff (42.0f, sampleRate);

    for (auto& dc : dcBlockers)
        dc.setCutoff (46.0f, sampleRate);

    shimmerDelayL.prepare (static_cast<int> (sampleRate * 0.30));
    shimmerDelayR.prepare (static_cast<int> (sampleRate * 0.30));
    shimmerDelaySamplesL = static_cast<float> (sampleRate * 0.081);
    shimmerDelaySamplesR = static_cast<float> (sampleRate * 0.107);

    shimmerInputToneL.setCutoff (8200.0f, sampleRate);
    shimmerInputToneR.setCutoff (8200.0f, sampleRate);
    shimmerLoopToneL.setCutoff (6800.0f, sampleRate);
    shimmerLoopToneR.setCutoff (6800.0f, sampleRate);
    shimmerHighpassL.setCutoff (95.0f, sampleRate);
    shimmerHighpassR.setCutoff (95.0f, sampleRate);

    octaveL.prepare (sampleRate, 0.043f);
    octaveR.prepare (sampleRate, 0.047f);

    sizeSmoothed.reset (sampleRate, 0.08);
    decaySmoothed.reset (sampleRate, 0.08);
    predelaySmoothed.reset (sampleRate, 0.05);
    dampingSmoothed.reset (sampleRate, 0.08);
    shimmerSmoothed.reset (sampleRate, 0.08);
    modulationSmoothed.reset (sampleRate, 0.08);
    freezeSmoothed.reset (sampleRate, 0.06);
    modeScaleSmoothed.reset (sampleRate, 0.12);
    modeDampingSmoothed.reset (sampleRate, 0.12);

    sizeSmoothed.setCurrentAndTargetValue (0.82f);
    decaySmoothed.setCurrentAndTargetValue (18.0f);
    predelaySmoothed.setCurrentAndTargetValue (45.0f);
    dampingSmoothed.setCurrentAndTargetValue (0.46f);
    shimmerSmoothed.setCurrentAndTargetValue (0.22f);
    modulationSmoothed.setCurrentAndTargetValue (0.72f);
    freezeSmoothed.setCurrentAndTargetValue (0.0f);
    modeScaleSmoothed.setCurrentAndTargetValue (modeScaleTable[2]);
    modeDampingSmoothed.setCurrentAndTargetValue (modeDampingTable[2]);

    mode = 2;
    lastFdnL = 0.0f;
    lastFdnR = 0.0f;
    controlCounter = 0;
    controlTicks = 0;
    randomState = 0x9e3779b9u;

    reset();
    updateControlRate();
    prepared = true;
}

void ReverbEngine::reset()
{
    for (auto& line : lines)
        line.reset();

    for (auto& filter : dampingFilters)
        filter.reset();

    for (auto& dc : dcBlockers)
        dc.reset();

    for (auto& allpass : diffusersL)
        allpass.reset();

    for (auto& allpass : diffusersR)
        allpass.reset();

    preDelayL.reset();
    preDelayR.reset();

    inputHighpassL.reset();
    inputHighpassR.reset();
    outputHighpassL.reset();
    outputHighpassR.reset();

    octaveL.reset();
    octaveR.reset();

    shimmerDelayL.reset();
    shimmerDelayR.reset();
    shimmerInputToneL.reset();
    shimmerInputToneR.reset();
    shimmerLoopToneL.reset();
    shimmerLoopToneR.reset();
    shimmerHighpassL.reset();
    shimmerHighpassR.reset();

    lastFdnL = 0.0f;
    lastFdnR = 0.0f;
    controlCounter = 0;
    controlTicks = 0;

    std::fill (modulationSmooth.begin(), modulationSmooth.end(), 0.0f);
    std::fill (modulationTarget.begin(), modulationTarget.end(), 0.0f);
}

void ReverbEngine::setParameters (const Parameters& params) noexcept
{
    const float size = std::clamp (params.size, 0.0f, 1.0f);
    const float decay = std::clamp (params.decaySeconds, 0.35f, 35.0f);
    const float predelay = std::clamp (params.predelayMs, 0.0f, 320.0f);
    const float damping = std::clamp (params.damping, 0.0f, 1.0f);
    const float shimmer = std::clamp (params.shimmer, 0.0f, 1.0f);
    const float modulation = std::clamp (params.modulation, 0.0f, 1.0f);

    sizeSmoothed.setTargetValue (size);
    decaySmoothed.setTargetValue (decay);
    predelaySmoothed.setTargetValue (predelay);
    dampingSmoothed.setTargetValue (damping);
    shimmerSmoothed.setTargetValue (shimmer);
    modulationSmoothed.setTargetValue (modulation);
    freezeSmoothed.setTargetValue (params.freeze ? 1.0f : 0.0f);

    const int newMode = std::clamp (params.mode, 0, 2);
    mode = newMode;
    modeScaleSmoothed.setTargetValue (modeScaleTable[newMode]);
    modeDampingSmoothed.setTargetValue (modeDampingTable[newMode]);
}

float ReverbEngine::randomBipolar() noexcept
{
    randomState = 1664525u * randomState + 1013904223u;
    const uint32_t bits = (randomState >> 8) & 0x00ffffffu;
    return static_cast<float> (bits) / 8388608.0f - 1.0f;
}

void ReverbEngine::hadamard (std::array<float, numLines>& values) noexcept
{
    float* v = values.data();

    for (int length = 1; length < numLines; length <<= 1)
    {
        for (int base = 0; base < numLines; base += length * 2)
        {
            for (int j = 0; j < length; ++j)
            {
                const float a = v[base + j];
                const float b = v[base + j + length];
                v[base + j] = a + b;
                v[base + j + length] = a - b;
            }
        }
    }

    for (int i = 0; i < numLines; ++i)
        v[i] *= hadamardScale;
}

void ReverbEngine::updateControlRate()
{
    const float scale = 0.55f + 1.1f * sizeSmoothed.getCurrentValue();
    const float modeScale = modeScaleSmoothed.getCurrentValue();
    const float decaySeconds = std::max (0.35f, decaySmoothed.getCurrentValue());
    const float freeze = std::clamp (freezeSmoothed.getCurrentValue(), 0.0f, 1.0f);
    const float damping = std::clamp (dampingSmoothed.getCurrentValue(), 0.0f, 1.0f);
    const float modeDamping = modeDampingSmoothed.getCurrentValue();

    float dampingCutoff = 19500.0f * std::pow (0.055f, damping) * modeDamping;
    dampingCutoff = std::clamp (dampingCutoff, 780.0f, 19500.0f);

    const float diffusion = modeDiffusionTable[std::clamp (mode, 0, 2)];

    for (int i = 0; i < numDiffusers; ++i)
    {
        diffusersL[static_cast<size_t> (i)].setFeedback (diffusion);
        diffusersR[static_cast<size_t> (i)].setFeedback (diffusion * 0.985f);
    }

    for (int i = 0; i < numLines; ++i)
    {
        const float delaySeconds = baseDelaySamples[static_cast<size_t> (i)] * scale * modeScale
                                   / static_cast<float> (sampleRate);
        float gain = std::exp (-ln1000 * delaySeconds / decaySeconds);
        gain += freeze * (0.99935f - gain);
        feedbackGain[static_cast<size_t> (i)] = std::clamp (gain, 0.0f, 0.99935f);

        const float lineDamping = dampingCutoff * (0.88f + 0.035f * static_cast<float> (i));
        dampingFilters[static_cast<size_t> (i)].setCutoff (lineDamping, sampleRate);
    }

    // New destinations roughly every 5 ms, then a very slow random-walk glide.
    // This gives the tail organic movement without audible pitch wobble.
    if ((controlTicks++ & 0x1f) == 0)
        for (int i = 0; i < numLines; ++i)
            modulationTarget[static_cast<size_t> (i)] = randomBipolar();
}

void ReverbEngine::processSample (float inputL, float inputR,
                                  float& wetL, float& wetR) noexcept
{
    wetL = 0.0f;
    wetR = 0.0f;

    if (! prepared)
        return;

    if (controlCounter == 0)
        updateControlRate();

    const float size = sizeSmoothed.getNextValue();
    const float modeScale = modeScaleSmoothed.getNextValue();
    const float decayNow = decaySmoothed.getNextValue();
    const float dampingNow = dampingSmoothed.getNextValue();
    const float modeDampingNow = modeDampingSmoothed.getNextValue();
    juce::ignoreUnused (decayNow, dampingNow, modeDampingNow);
    const float predelaySamples = predelaySmoothed.getNextValue() * 0.001f * static_cast<float> (sampleRate);
    const float modulationDepth = modulationSmoothed.getNextValue() * static_cast<float> (sampleRate) * 0.0017f;
    const float shimmerAmount = std::clamp (shimmerSmoothed.getNextValue(), 0.0f, 1.0f);
    const float freeze = std::clamp (freezeSmoothed.getNextValue(), 0.0f, 1.0f);
    const float sendGain = 1.0f - freeze;
    const float delayScale = (0.55f + 1.1f * size) * modeScale;

    for (int i = 0; i < numLines; ++i)
    {
        auto& smooth = modulationSmooth[static_cast<size_t> (i)];
        smooth += 0.0018f * (modulationTarget[static_cast<size_t> (i)] - smooth);
        const float delay = baseDelaySamples[static_cast<size_t> (i)] * delayScale + smooth * modulationDepth;
        delaySamples[static_cast<size_t> (i)] = std::clamp (delay, 12.0f,
                                                            static_cast<float> (maxDelaySamples - 4));
    }

    const float highpassedL = inputHighpassL.process (inputL);
    const float highpassedR = inputHighpassR.process (inputR);

    preDelayL.write (highpassedL);
    preDelayR.write (highpassedR);

    float diffusedL = preDelayL.read (predelaySamples) * sendGain;
    float diffusedR = preDelayR.read (predelaySamples) * sendGain;

    for (int i = 0; i < numDiffusers; ++i)
    {
        diffusedL = diffusersL[static_cast<size_t> (i)].process (diffusedL);
        diffusedR = diffusersR[static_cast<size_t> (i)].process (diffusedR);
    }

    // Parallel octave-up shimmer network, fed by the previous FDN output.
    const float shimmerSend = shimmerAmount * 0.58f;
    const float shimmerFeedback = 0.32f + 0.28f * shimmerAmount;
    const float shimmerOutputGain = shimmerAmount * 0.55f;

    const float shimmerMod = modulationSmooth[0] * static_cast<float> (sampleRate) * 0.00035f;

    const float shimmerReadL = shimmerDelayL.read (shimmerDelaySamplesL + shimmerMod);
    const float shimmerReadR = shimmerDelayR.read (shimmerDelaySamplesR - shimmerMod * 0.8f);

    float shimmerInL = lastFdnL * shimmerSend + shimmerReadL * shimmerFeedback;
    float shimmerInR = lastFdnR * shimmerSend + shimmerReadR * shimmerFeedback;

    shimmerInL = shimmerInputToneL.process (shimmerInL);
    shimmerInR = shimmerInputToneR.process (shimmerInR);

    float shiftedL = octaveL.process (shimmerInL);
    float shiftedR = octaveR.process (shimmerInR);

    shiftedL = shimmerLoopToneL.process (shiftedL);
    shiftedR = shimmerLoopToneR.process (shiftedR);
    shiftedL = shimmerHighpassL.process (shiftedL);
    shiftedR = shimmerHighpassR.process (shiftedR);

    // Gentle saturation keeps repeated octave generations musical and bounded.
    shiftedL = std::tanh (shiftedL * 1.15f) * 0.869565f;
    shiftedR = std::tanh (shiftedR * 1.15f) * 0.869565f;

    shimmerDelayL.write (shiftedL);
    shimmerDelayR.write (shiftedR);

    const float shimmerL = shiftedL * shimmerOutputGain;
    const float shimmerR = shiftedR * shimmerOutputGain;

    std::array<float, numLines> delayed {};
    std::array<float, numLines> work {};

    for (int i = 0; i < numLines; ++i)
        delayed[static_cast<size_t> (i)] = lines[static_cast<size_t> (i)].read (delaySamples[static_cast<size_t> (i)]);

    for (int i = 0; i < numLines; ++i)
    {
        float value = dampingFilters[static_cast<size_t> (i)].process (delayed[static_cast<size_t> (i)]);
        value = dcBlockers[static_cast<size_t> (i)].process (value);
        work[static_cast<size_t> (i)] = value * feedbackGain[static_cast<size_t> (i)];
    }

    hadamard (work);

    const float injectL = diffusedL * 0.38f;
    const float injectR = diffusedR * 0.38f;

    const std::array<float, numLines> injection {
        injectL, injectR, injectL, -injectR,
        injectL, injectR, -injectL, injectR
    };

    for (int i = 0; i < numLines; ++i)
        lines[static_cast<size_t> (i)].write (work[static_cast<size_t> (i)]
                                              + injection[static_cast<size_t> (i)]);

    const float fdnL = 0.30f * (delayed[0] + delayed[2] - delayed[5] + delayed[7]);
    const float fdnR = 0.30f * (delayed[1] - delayed[3] + delayed[4] + delayed[6]);

    lastFdnL = fdnL;
    lastFdnR = fdnR;

    wetL = outputHighpassL.process (fdnL + shimmerL);
    wetR = outputHighpassR.process (fdnR + shimmerR);

    if (! std::isfinite (wetL) || ! std::isfinite (wetR))
    {
        reset();
        wetL = 0.0f;
        wetR = 0.0f;
    }

    wetL = std::clamp (wetL, -16.0f, 16.0f);
    wetR = std::clamp (wetR, -16.0f, 16.0f);

    if (++controlCounter >= 16)
        controlCounter = 0;
}

} // namespace geverb::dsp
