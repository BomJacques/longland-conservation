#pragma once

#include "DSP/ConsequenceProcessor.h"
#include "DSP/ReflectionProcessor.h"
#include "Physics/ConservationEngine.h"
#include "Physics/SnapshotExchange.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

class LonglandConservationAudioProcessor final : public juce::AudioProcessor
{
public:
    LonglandConservationAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 65.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    [[nodiscard]] longland::physics::PhysicsSnapshot consumePhysicsSnapshot() noexcept
    {
        return snapshotExchange.consume();
    }

    juce::AudioProcessorValueTreeState parameters;
    static constexpr std::array<const char*, 10> parameterIds {
        "mass", "time", "gravity", "pressure", "heat",
        "elasticity", "friction", "dispersion", "instability", "energy"
    };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    longland::physics::ConservationEngine physicsEngine;
    longland::physics::SnapshotExchange snapshotExchange;
    longland::dsp::ReflectionProcessor reflectionProcessor;
    longland::dsp::ConsequenceProcessor consequenceProcessor;
    std::array<std::atomic<float>*, parameterIds.size()> parameterValues {};
    std::atomic<bool> restoreRequested { false };
    double currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LonglandConservationAudioProcessor)
};
