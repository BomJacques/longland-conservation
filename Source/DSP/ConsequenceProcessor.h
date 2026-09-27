#pragma once

#include "DSPMapper.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

namespace longland::dsp
{
// Lightweight Phase 4-7 renderer. Every stage is attenuation-only at its
// displaced extreme; no stage can independently add broadband level.
class ConsequenceProcessor
{
public:
    void prepare (double newSampleRate, int channels) noexcept;
    void reset() noexcept;
    void setTargets (const DSPTargets& targets) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    struct SmoothValue
    {
        float current { 1.0f };
        float target { 1.0f };
        float coefficient { 0.99f };
        float next() noexcept { return current += (target - current) * (1.0f - coefficient); }
    };

    static constexpr int maxChannels = 2;
    double sampleRate { 44100.0 };
    int preparedChannels { 1 };
    float lowCoefficient {};
    float highCoefficient {};
    float envelopeFastAttack {}, envelopeFastRelease {};
    float envelopeSlowAttack {}, envelopeSlowRelease {};
    std::array<float, maxChannels> lowState {};
    std::array<float, maxChannels> highLowpassState {};
    std::array<float, maxChannels> fastEnvelope {};
    std::array<float, maxChannels> slowEnvelope {};
    SmoothValue transientGain, sustainGain, lowGain, midBandGain, highGain;
    SmoothValue harmonicBlend, harmonicDrive, stereoMidGain, stereoSideGain;
};
} // namespace longland::dsp

