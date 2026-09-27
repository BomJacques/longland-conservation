#pragma once

#include "PluginProcessor.h"
#include "UI/EnergyFieldComponent.h"
#include "UI/PhysicsDebugPanel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

class LonglandConservationAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                        private juce::Timer
{
public:
    explicit LonglandConservationAudioProcessorEditor (LonglandConservationAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    LonglandConservationAudioProcessor& processor;
    longland::ui::EnergyFieldComponent energyField;
    longland::ui::PhysicsDebugPanel debugPanel;
    std::array<juce::Slider, LonglandConservationAudioProcessor::parameterIds.size()> sliders;
    std::array<juce::Label, LonglandConservationAudioProcessor::parameterIds.size()> labels;
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::array<std::unique_ptr<Attachment>, LonglandConservationAudioProcessor::parameterIds.size()> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LonglandConservationAudioProcessorEditor)
};

