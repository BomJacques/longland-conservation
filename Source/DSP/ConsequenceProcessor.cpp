#include "ConsequenceProcessor.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace longland::dsp
{
namespace
{
[[nodiscard]] float coefficientFor (double sampleRate, double seconds) noexcept
{
    return static_cast<float> (std::exp (-1.0 / std::max (1.0, sampleRate * seconds)));
}

[[nodiscard]] float finiteClamped (double value, float fallback, float low, float high) noexcept
{
    return std::isfinite (value) ? std::clamp (static_cast<float> (value), low, high) : fallback;
}
} // namespace

void ConsequenceProcessor::prepare (double newSampleRate, int channels) noexcept
{
    sampleRate = std::isfinite (newSampleRate) ? std::clamp (newSampleRate, 8000.0, 384000.0) : 44100.0;
    preparedChannels = std::clamp (channels, 1, maxChannels);
    lowCoefficient = static_cast<float> (std::exp (-2.0 * std::numbers::pi * 250.0 / sampleRate));
    highCoefficient = static_cast<float> (std::exp (-2.0 * std::numbers::pi * 3000.0 / sampleRate));
    envelopeFastAttack = coefficientFor (sampleRate, 0.001);
    envelopeFastRelease = coefficientFor (sampleRate, 0.030);
    envelopeSlowAttack = coefficientFor (sampleRate, 0.030);
    envelopeSlowRelease = coefficientFor (sampleRate, 0.250);
    const auto targetSmoothing = coefficientFor (sampleRate, 0.050);
    for (auto* value : { &transientGain, &sustainGain, &lowGain, &midBandGain, &highGain,
                         &harmonicBlend, &harmonicDrive, &stereoMidGain, &stereoSideGain })
        value->coefficient = targetSmoothing;
    harmonicBlend.current = harmonicBlend.target = 0.0f;
    harmonicDrive.current = harmonicDrive.target = 1.0f;
    reset();
}

void ConsequenceProcessor::reset() noexcept
{
    lowState.fill (0.0f);
    highLowpassState.fill (0.0f);
    fastEnvelope.fill (0.0f);
    slowEnvelope.fill (0.0f);
    for (auto* value : { &transientGain, &sustainGain, &lowGain, &midBandGain, &highGain,
                         &harmonicBlend, &harmonicDrive, &stereoMidGain, &stereoSideGain })
        value->current = value->target;
}

void ConsequenceProcessor::setTargets (const DSPTargets& targets) noexcept
{
    transientGain.target = finiteClamped (targets.transientSegmentGain, 1.0f, 0.0f, 1.0f);
    sustainGain.target = finiteClamped (targets.sustainSegmentGain, 1.0f, 0.0f, 1.0f);
    lowGain.target = finiteClamped (targets.spectralBandGains[0], 1.0f, 0.0f, 1.0f);
    midBandGain.target = finiteClamped (targets.spectralBandGains[1], 1.0f, 0.0f, 1.0f);
    highGain.target = finiteClamped (targets.spectralBandGains[2], 1.0f, 0.0f, 1.0f);
    harmonicBlend.target = finiteClamped (targets.harmonicBlend, 0.0f, 0.0f, 1.0f);
    harmonicDrive.target = finiteClamped (targets.harmonicDrive, 1.0f, 1.0f, 16.0f);
    stereoMidGain.target = finiteClamped (targets.midGain, 1.0f, 0.0f, 1.0f);
    stereoSideGain.target = finiteClamped (targets.sideGain, 1.0f, 0.0f, 1.0f);
}

void ConsequenceProcessor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto channels = std::min (preparedChannels, buffer.getNumChannels());
    const auto samples = buffer.getNumSamples();
    if (channels <= 0 || samples <= 0)
        return;

    std::array<float*, maxChannels> data {};
    for (int channel = 0; channel < channels; ++channel)
        data[static_cast<std::size_t> (channel)] = buffer.getWritePointer (channel);

    for (int sample = 0; sample < samples; ++sample)
    {
        const auto attackTarget = transientGain.next();
        const auto sustainTarget = sustainGain.next();
        const auto lowTarget = lowGain.next();
        const auto midTarget = midBandGain.next();
        const auto highTarget = highGain.next();
        const auto blend = harmonicBlend.next();
        const auto drive = harmonicDrive.next();
        const auto centre = stereoMidGain.next();
        const auto side = stereoSideGain.next();

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto slot = static_cast<std::size_t> (channel);
            auto x = std::isfinite (data[slot][sample]) ? data[slot][sample] : 0.0f;
            const auto magnitude = std::abs (x);
            const auto fastC = magnitude > fastEnvelope[slot] ? envelopeFastAttack : envelopeFastRelease;
            const auto slowC = magnitude > slowEnvelope[slot] ? envelopeSlowAttack : envelopeSlowRelease;
            fastEnvelope[slot] = fastC * fastEnvelope[slot] + (1.0f - fastC) * magnitude;
            slowEnvelope[slot] = slowC * slowEnvelope[slot] + (1.0f - slowC) * magnitude;
            const auto transientMask = std::clamp (
                (fastEnvelope[slot] - slowEnvelope[slot]) / (fastEnvelope[slot] + 1.0e-9f), 0.0f, 1.0f);
            x *= sustainTarget + transientMask * (attackTarget - sustainTarget);

            lowState[slot] = lowCoefficient * lowState[slot] + (1.0f - lowCoefficient) * x;
            highLowpassState[slot] = highCoefficient * highLowpassState[slot]
                                     + (1.0f - highCoefficient) * x;
            const auto low = lowState[slot];
            const auto mid = highLowpassState[slot] - low;
            const auto high = x - highLowpassState[slot];
            x = lowTarget * low + midTarget * mid + highTarget * high;

            const auto shaped = std::tanh (drive * x) / drive;
            data[slot][sample] = std::isfinite (shaped)
                ? x + blend * (shaped - x) : 0.0f;
        }

        if (channels == 2)
        {
            const auto left = data[0][sample];
            const auto right = data[1][sample];
            const auto mid = 0.5f * (left + right) * centre;
            const auto sides = 0.5f * (left - right) * side;
            data[0][sample] = mid + sides;
            data[1][sample] = mid - sides;
        }
    }
}
} // namespace longland::dsp
