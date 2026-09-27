#include "PluginEditor.h"

namespace
{
constexpr std::array<const char*, 10> displayNames {
    "MASS", "TIME", "GRAVITY", "PRESSURE", "HEAT",
    "ELASTICITY", "FRICTION", "DISPERSION", "INSTABILITY", "ENERGY"
};
}

LonglandConservationAudioProcessorEditor::LonglandConservationAudioProcessorEditor (
    LonglandConservationAudioProcessor& owner)
    : AudioProcessorEditor (&owner), processor (owner)
{
    addAndMakeVisible (energyField);
    addAndMakeVisible (debugPanel);
    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        auto& slider = sliders[i];
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 18);
        slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff66e0cf));
        slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xfff4d47c));
        addAndMakeVisible (slider);

        auto& label = labels[i];
        label.setText (displayNames[i], juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colour (0xffcad8d7));
        addAndMakeVisible (label);
        attachments[i] = std::make_unique<Attachment> (
            processor.parameters, LonglandConservationAudioProcessor::parameterIds[i], slider);
    }
    setResizable (true, true);
    setResizeLimits (760, 620, 1400, 1100);
    setSize (980, 760);
    timerCallback();
    startTimerHz (30);
}

void LonglandConservationAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff071112));
    g.setColour (juce::Colour (0xffe8f2ef));
    g.setFont (juce::FontOptions { 22.0f, juce::Font::bold });
    g.drawText ("LONGLAND CONSERVATION", 20, 10, getWidth() - 40, 30, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff738684));
    g.setFont (juce::FontOptions { 12.0f });
    g.drawText ("NOTHING HAPPENS FOR FREE", 20, 38, getWidth() - 40, 18, juce::Justification::centredLeft);
}

void LonglandConservationAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16);
    bounds.removeFromTop (48);
    const auto debugWidth = std::clamp (bounds.getWidth() / 3, 245, 340);
    auto debug = bounds.removeFromRight (debugWidth).reduced (6);
    debugPanel.setBounds (debug);

    auto controls = bounds.removeFromBottom (230).reduced (4);
    energyField.setBounds (bounds.reduced (4));
    const auto cellWidth = controls.getWidth() / 5;
    const auto cellHeight = controls.getHeight() / 2;
    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        const auto column = static_cast<int> (i % 5);
        const auto row = static_cast<int> (i / 5);
        auto cell = juce::Rectangle<int> (controls.getX() + column * cellWidth,
                                          controls.getY() + row * cellHeight,
                                          cellWidth, cellHeight).reduced (4);
        labels[i].setBounds (cell.removeFromTop (18));
        sliders[i].setBounds (cell);
    }
}

void LonglandConservationAudioProcessorEditor::timerCallback()
{
    const auto snapshot = processor.consumePhysicsSnapshot();
    energyField.setSnapshot (snapshot);
    debugPanel.setSnapshot (snapshot);
}
