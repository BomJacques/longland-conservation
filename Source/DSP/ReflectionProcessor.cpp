#include "ReflectionProcessor.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace longland::dsp
{
void ReflectionProcessor::Smoothed::setTime (double sr, double seconds) noexcept
{
    coefficient = static_cast<float> (std::exp (-1.0 / std::max (1.0, sr * seconds)));
}

void ReflectionProcessor::prepare (double newSampleRate, int, int channels)
{
    sampleRate = std::isfinite (newSampleRate) ? std::clamp (newSampleRate, 8000.0, 384000.0) : 44100.0;
    preparedChannels = std::clamp (channels, 1, maxChannels);
    ringSize = static_cast<int> (std::ceil (sampleRate * 1.0)) + 4;
    dcCoefficient = static_cast<float> (std::exp (-2.0 * std::numbers::pi * 10.0 / sampleRate));
    for (auto& ring : inputDelay)
        ring.assign (static_cast<std::size_t> (ringSize), 0.0f);
    for (auto& ring : wetDelay)
        ring.assign (static_cast<std::size_t> (ringSize), 0.0f);

    for (auto* smoother : { &dryGain, &wetInputGain, &feedback, &damping, &outputGain })
        smoother->setTime (sampleRate, 0.020);
    delaySamples.setTime (sampleRate, 0.050);

    dryGain.current = dryGain.target = 1.0f;
    wetInputGain.current = wetInputGain.target = 0.0f;
    feedback.current = feedback.target = 0.15f;
    delaySamples.current = delaySamples.target = static_cast<float> (sampleRate * 0.12);
    damping.current = damping.target = 0.3f;
    outputGain.current = outputGain.target = 1.0f;
    reset();
}

void ReflectionProcessor::reset() noexcept
{
    for (auto& ring : inputDelay)
        std::fill (ring.begin(), ring.end(), 0.0f);
    for (auto& ring : wetDelay)
        std::fill (ring.begin(), ring.end(), 0.0f);
    loopLowpass.fill (0.0f);
    dcInput.fill (0.0f);
    dcOutput.fill (0.0f);
    writePosition = 0;
    limiterGain = 1.0f;
    for (auto* smoother : { &dryGain, &wetInputGain, &feedback, &delaySamples, &damping, &outputGain })
        smoother->current = smoother->target;
}

void ReflectionProcessor::setTargets (const DSPTargets& targets) noexcept
{
    const auto finiteOr = [] (double value, double fallback) noexcept {
        return std::isfinite (value) ? value : fallback;
    };
    // Phase 3 renders the complete reflected share through delay. When the FDN
    // arrives it will consume its partition instead of duplicating it.
    const auto delayOnly = DSPMapper::makeConservedDelayGains (
        1.0 - finiteOr (targets.directPower, 1.0), 1.0,
        finiteOr (targets.reflection.feedback, 0.0), 0.92);
    dryGain.target = static_cast<float> (std::clamp (delayOnly.direct, 0.0, 1.0));
    wetInputGain.target = static_cast<float> (std::clamp (delayOnly.delayInput, 0.0, 1.0));
    feedback.target = static_cast<float> (std::clamp (delayOnly.feedback, 0.0, 0.92));
    delaySamples.target = std::clamp (
        static_cast<float> (finiteOr (targets.delaySeconds, 0.12) * sampleRate),
        1.0f, static_cast<float> (std::max (1, ringSize - 3)));
    const auto cutoff = std::clamp (finiteOr (targets.reflectionLowpassHz, 8000.0), 20.0, sampleRate * 0.45);
    damping.target = static_cast<float> (std::exp (-2.0 * std::numbers::pi * cutoff / sampleRate));
    outputGain.target = static_cast<float> (std::clamp (finiteOr (targets.outputGain, 1.0), 0.0, 1.0));
}

float ReflectionProcessor::readLinear (const std::vector<float>& ring, float delay) const noexcept
{
    if (! std::isfinite (delay) || ring.size() < static_cast<std::size_t> (ringSize))
        return 0.0f;
    auto position = static_cast<float> (writePosition) - delay;
    while (position < 0.0f)
        position += static_cast<float> (ringSize);
    const auto first = static_cast<int> (position) % ringSize;
    const auto second = (first + 1) % ringSize;
    const auto fraction = position - std::floor (position);
    return ring[static_cast<std::size_t> (first)]
        + fraction * (ring[static_cast<std::size_t> (second)] - ring[static_cast<std::size_t> (first)]);
}

void ReflectionProcessor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto channels = std::min (preparedChannels, buffer.getNumChannels());
    const auto samples = buffer.getNumSamples();
    if (channels <= 0 || samples <= 0 || ringSize < 4)
        return;

    constexpr float ceiling = 0.8912509f; // -1 dBFS sample peak safety ceiling.
    const auto release = static_cast<float> (std::exp (-1.0 / (sampleRate * 0.100)));
    std::array<float*, maxChannels> channelData {};
    for (int channel = 0; channel < channels; ++channel)
        channelData[static_cast<std::size_t> (channel)] = buffer.getWritePointer (channel);

    for (int sample = 0; sample < samples; ++sample)
    {
        const auto kd = dryGain.next();
        const auto kw = wetInputGain.next();
        const auto g = feedback.next();
        const auto delay = delaySamples.next();
        const auto damp = damping.next();
        const auto systemGain = outputGain.next();
        float framePeak = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = channelData[static_cast<std::size_t> (channel)];
            const auto input = std::isfinite (data[sample]) ? data[sample] : 0.0f;
            const auto delayedInput = readLinear (inputDelay[static_cast<std::size_t> (channel)], delay);
            const auto delayedWet = readLinear (wetDelay[static_cast<std::size_t> (channel)], delay);
            const auto slot = static_cast<std::size_t> (channel);
            const auto dcBlocked = delayedWet - dcInput[slot] + dcCoefficient * dcOutput[slot];
            dcInput[slot] = delayedWet;
            dcOutput[slot] = std::isfinite (dcBlocked) ? dcBlocked : 0.0f;
            loopLowpass[slot] += (dcOutput[slot] - loopLowpass[slot]) * (1.0f - damp);
            const auto wet = kw * delayedInput + g * loopLowpass[static_cast<std::size_t> (channel)];

            inputDelay[static_cast<std::size_t> (channel)][static_cast<std::size_t> (writePosition)] = input;
            wetDelay[static_cast<std::size_t> (channel)][static_cast<std::size_t> (writePosition)] =
                std::isfinite (wet) ? std::clamp (wet, -8.0f, 8.0f) : 0.0f;
            data[sample] = systemGain * (kd * input + wet);
            framePeak = std::max (framePeak, std::abs (data[sample]));
        }

        const auto required = framePeak > ceiling ? ceiling / framePeak : 1.0f;
        const auto releaseRecovery = 1.0f - release * (1.0f - limiterGain);
        limiterGain = std::min (required, releaseRecovery);
        for (int channel = 0; channel < channels; ++channel)
            channelData[static_cast<std::size_t> (channel)][sample] *= limiterGain;

        writePosition = (writePosition + 1) % ringSize;
    }

    for (int channel = channels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, samples);
}
} // namespace longland::dsp
