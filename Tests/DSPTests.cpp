#include "../Source/DSP/DSPMapper.h"
#include "../Source/DSP/ConsequenceProcessor.h"
#include "../Source/DSP/ReflectionProcessor.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

using namespace longland;

namespace
{
int failures = 0;

void check (bool condition, std::string_view message)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

dsp::DSPTargets maximumReflectionTargets()
{
    dsp::DSPTargets targets;
    targets.directPower = 0.0;
    targets.reflection.feedback = 0.92;
    targets.delaySeconds = 0.03;
    targets.reflectionLowpassHz = 18000.0;
    return targets;
}

std::vector<float> renderImpulse (double sampleRate, int blockSize, int outputSamples)
{
    dsp::ReflectionProcessor processor;
    processor.prepare (sampleRate, blockSize, 1);
    processor.setTargets (maximumReflectionTargets());

    // Let parameter smoothing settle without adding energy to the delay lines.
    auto settle = static_cast<int> (sampleRate * 0.15);
    while (settle > 0)
    {
        const auto count = std::min (blockSize, settle);
        juce::AudioBuffer<float> silence (1, count);
        silence.clear();
        processor.process (silence);
        settle -= count;
    }

    std::vector<float> output (static_cast<std::size_t> (outputSamples), 0.0f);
    auto position = 0;
    while (position < outputSamples)
    {
        const auto count = std::min (blockSize, outputSamples - position);
        juce::AudioBuffer<float> block (1, count);
        block.clear();
        if (position == 0)
            block.setSample (0, 0, 1.0f);
        processor.process (block);
        std::copy_n (block.getReadPointer (0), count, output.begin() + position);
        position += count;
    }
    return output;
}

void mapperConservesDelayTailPower()
{
    for (const auto feedback : { 0.0, 0.25, 0.75, 0.92, 5.0 })
    {
        const auto gains = dsp::DSPMapper::makeConservedDelayGains (0.7, 1.0, feedback, 0.92);
        const auto geometricTailPower = gains.delayInput * gains.delayInput
                                      / (1.0 - gains.feedback * gains.feedback);
        check (std::abs (gains.direct * gains.direct + geometricTailPower - 1.0) < 1.0e-12,
               "delay gain mapping must conserve analytic impulse energy");
        check (gains.feedback <= 0.92, "feedback mapping must enforce a hard upper bound");
    }
}

void impulseTailIsFiniteAndBounded()
{
    constexpr float sampleCeiling = 0.8912509f;
    for (const auto sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        const auto output = renderImpulse (sampleRate, 127, static_cast<int> (sampleRate * 2.0));
        float peak = 0.0f;
        for (const auto sample : output)
        {
            check (std::isfinite (sample), "reflection impulse response must remain finite");
            peak = std::max (peak, std::abs (sample));
        }
        check (peak <= sampleCeiling + 1.0e-6f, "reflection safety limiter must bound sample peaks");

        // Delay time slews from the prepared 120 ms value toward the 30 ms
        // target. Assert the audible event within that legal transition window
        // instead of pretending it jumps to an exact integer sample.
        const auto first = static_cast<std::size_t> (sampleRate * 0.02);
        const auto last = std::min (output.size(), static_cast<std::size_t> (sampleRate * 0.15));
        const auto delayedPeak = *std::max_element (output.begin() + static_cast<std::ptrdiff_t> (first),
                                                    output.begin() + static_cast<std::ptrdiff_t> (last),
                                                    [] (float a, float b) { return std::abs (a) < std::abs (b); });
        check (std::abs (delayedPeak) > 1.0e-4f,
               "maximum reflected state must produce a real delayed impulse during the delay slew");
    }
}

void blockPartitionDoesNotChangeDSP()
{
    const auto reference = renderImpulse (48000.0, 1, 48000);
    for (const auto blockSize : { 7, 31, 64, 127, 256, 511, 1024 })
    {
        const auto candidate = renderImpulse (48000.0, blockSize, 48000);
        double maximumDifference = 0.0;
        for (std::size_t i = 0; i < reference.size(); ++i)
            maximumDifference = std::max (maximumDifference,
                                          std::abs (static_cast<double> (reference[i] - candidate[i])));
        check (maximumDifference < 1.0e-7, "reflection trajectory must be buffer-size invariant");
    }
}

void hostileInputsAndTargetsAreSanitised()
{
    dsp::ReflectionProcessor processor;
    processor.prepare (48000.0, 64, 2);
    auto targets = maximumReflectionTargets();
    targets.directPower = std::numeric_limits<double>::quiet_NaN();
    targets.reflection.feedback = std::numeric_limits<double>::infinity();
    targets.delaySeconds = -std::numeric_limits<double>::infinity();
    targets.reflectionLowpassHz = std::numeric_limits<double>::quiet_NaN();
    targets.outputGain = std::numeric_limits<double>::quiet_NaN();
    processor.setTargets (targets);

    juce::AudioBuffer<float> buffer (2, 64);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* samples = buffer.getWritePointer (channel);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            samples[i] = i % 2 == 0 ? std::numeric_limits<float>::quiet_NaN()
                                    : std::numeric_limits<float>::infinity();
    }
    processor.process (buffer);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            check (std::isfinite (buffer.getSample (channel, i)),
                   "non-finite automation/input must never reach plugin output");
}

void silenceStaysSilent()
{
    dsp::ReflectionProcessor processor;
    processor.prepare (48000.0, 511, 2);
    processor.setTargets (maximumReflectionTargets());
    for (int blockIndex = 0; blockIndex < 200; ++blockIndex)
    {
        juce::AudioBuffer<float> buffer (2, 511);
        buffer.clear();
        processor.process (buffer);
        check (buffer.getMagnitude (0, buffer.getNumSamples()) == 0.0f,
               "reflection processor must not mint audio from digital silence");
    }
}

double consequenceRms (const dsp::DSPTargets& targets)
{
    constexpr double sampleRate = 48000.0;
    dsp::ConsequenceProcessor processor;
    processor.prepare (sampleRate, 2);
    processor.setTargets (targets);

    juce::AudioBuffer<float> settle (2, 24000);
    settle.clear();
    processor.process (settle);

    juce::AudioBuffer<float> signal (2, 48000);
    for (int i = 0; i < signal.getNumSamples(); ++i)
    {
        const auto sample = 0.125f * std::sin (static_cast<float> (
            2.0 * 3.14159265358979323846 * 997.0 * static_cast<double> (i) / sampleRate));
        signal.setSample (0, i, sample);
        signal.setSample (1, i, sample);
    }
    processor.process (signal);
    return signal.getRMSLevel (0, 0, signal.getNumSamples());
}

void mappedConsequencesReachAudio()
{
    const auto neutralState = physics::EnergyState::equilibrium();
    const auto neutralRms = consequenceRms (dsp::DSPMapper::map (neutralState));

    auto pressureState = neutralState;
    pressureState[physics::Reservoir::harmonic] += 0.10;
    pressureState[physics::Reservoir::direct] -= 0.10;
    const auto pressureRms = consequenceRms (dsp::DSPMapper::map (pressureState));
    check (pressureRms < neutralRms * 0.99,
           "harmonic energy must audibly cost clean programme level/precision");

    auto dispersedState = neutralState;
    dispersedState[physics::Reservoir::spatial] += 0.10;
    dispersedState[physics::Reservoir::direct] -= 0.10;
    const auto dispersedRms = consequenceRms (dsp::DSPMapper::map (dispersedState));
    check (dispersedRms < neutralRms * 0.99,
           "spatial energy must audibly weaken a mono-centre signal");
}

void limiterNeverOvershootsWhileRecovering()
{
    constexpr double sampleRate = 48000.0;
    constexpr float ceiling = 0.8912509f;
    dsp::ReflectionProcessor processor;
    processor.prepare (sampleRate, 2, 1);
    dsp::DSPTargets direct;
    direct.directPower = 1.0;
    direct.outputGain = 1.0;
    processor.setTargets (direct);
    processor.reset();

    const auto release = static_cast<float> (std::exp (-1.0 / (sampleRate * 0.100)));
    const auto firstGain = ceiling / 4.0f;
    const auto recovery = 1.0f - release * (1.0f - firstGain);
    const auto narrowRequiredGain = 0.5f * (firstGain + recovery);

    juce::AudioBuffer<float> buffer (1, 2);
    buffer.setSample (0, 0, 4.0f);
    buffer.setSample (0, 1, ceiling / narrowRequiredGain);
    processor.process (buffer);
    check (std::abs (buffer.getSample (0, 0)) <= ceiling + 1.0e-6f
               && std::abs (buffer.getSample (0, 1)) <= ceiling + 1.0e-6f,
           "limiter recovery must never step above the current sample's required ceiling gain");
}
} // namespace

int main()
{
    mapperConservesDelayTailPower();
    impulseTailIsFiniteAndBounded();
    blockPartitionDoesNotChangeDSP();
    hostileInputsAndTargetsAreSanitised();
    silenceStaysSilent();
    mappedConsequencesReachAudio();
    limiterNeverOvershootsWhileRecovering();

    if (failures != 0)
    {
        std::cerr << failures << " DSP assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Longland adversarial DSP tests passed.\n";
    return EXIT_SUCCESS;
}
