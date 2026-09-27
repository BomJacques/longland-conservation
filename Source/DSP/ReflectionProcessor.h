#pragma once

#include "DSPMapper.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <vector>

namespace longland::dsp
{
class ReflectionProcessor
{
public:
    void prepare (double newSampleRate, int maximumBlockSize, int channels);
    void reset() noexcept;
    void setTargets (const DSPTargets& newTargets) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    struct Smoothed
    {
        float current {};
        float target {};
        float coefficient { 0.99f };
        void setTime (double sampleRate, double seconds) noexcept;
        float next() noexcept { return current += (target - current) * (1.0f - coefficient); }
    };

    [[nodiscard]] float readLinear (const std::vector<float>& ring, float delaySamples) const noexcept;

    static constexpr int maxChannels = 2;
    double sampleRate { 44100.0 };
    int writePosition {};
    int ringSize { 2 };
    int preparedChannels { 1 };
    std::array<std::vector<float>, maxChannels> inputDelay;
    std::array<std::vector<float>, maxChannels> wetDelay;
    std::array<float, maxChannels> loopLowpass {};
    std::array<float, maxChannels> dcInput {};
    std::array<float, maxChannels> dcOutput {};
    float dcCoefficient { 0.998f };
    Smoothed dryGain, wetInputGain, feedback, delaySamples, damping, outputGain;
    float limiterGain { 1.0f };
};
} // namespace longland::dsp
