#include "PluginProcessor.h"
#include "PluginEditor.h"

LonglandConservationAudioProcessor::LonglandConservationAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "STATE", createParameterLayout())
{
    for (std::size_t i = 0; i < parameterIds.size(); ++i)
        parameterValues[i] = parameters.getRawParameterValue (parameterIds[i]);
    snapshotExchange.publish (physicsEngine.snapshot());
}

juce::AudioProcessorValueTreeState::ParameterLayout
LonglandConservationAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto add = [&layout] (const char* id, const char* name, float initial) {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0001f }, initial));
    };
    add ("mass", "Mass", 0.0f);
    add ("time", "Time", 0.0f);
    add ("gravity", "Gravity", 0.0f);
    add ("pressure", "Pressure", 0.0f);
    add ("heat", "Heat", 0.0f);
    add ("elasticity", "Elasticity", 0.35f);
    add ("friction", "Friction", 0.55f);
    add ("dispersion", "Dispersion", 0.0f);
    add ("instability", "Instability", 0.0f);
    add ("energy", "Energy", 1.0f);
    return layout;
}

void LonglandConservationAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = std::isfinite (sampleRate) ? std::clamp (sampleRate, 8000.0, 384000.0) : 44100.0;
    physicsEngine.prepare (100.0);
    reflectionProcessor.prepare (currentSampleRate, samplesPerBlock, getTotalNumOutputChannels());
    consequenceProcessor.prepare (currentSampleRate, getTotalNumOutputChannels());
}

void LonglandConservationAudioProcessor::releaseResources()
{
    reflectionProcessor.reset();
    consequenceProcessor.reset();
}

bool LonglandConservationAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && output == layouts.getMainInputChannelSet();
}

void LonglandConservationAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumSamples() <= 0)
        return;
    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    longland::physics::PhysicsControls controls;
    controls.mass = parameterValues[0]->load (std::memory_order_relaxed);
    controls.time = parameterValues[1]->load (std::memory_order_relaxed);
    controls.gravity = parameterValues[2]->load (std::memory_order_relaxed);
    controls.pressure = parameterValues[3]->load (std::memory_order_relaxed);
    controls.heat = parameterValues[4]->load (std::memory_order_relaxed);
    controls.elasticity = parameterValues[5]->load (std::memory_order_relaxed);
    controls.friction = parameterValues[6]->load (std::memory_order_relaxed);
    controls.dispersion = parameterValues[7]->load (std::memory_order_relaxed);
    controls.instability = parameterValues[8]->load (std::memory_order_relaxed);
    controls.energy = parameterValues[9]->load (std::memory_order_relaxed);

    if (restoreRequested.exchange (false, std::memory_order_acq_rel))
    {
        physicsEngine.reset (controls.energy);
        longland::dsp::MappingConfiguration restoredConfiguration;
        restoredConfiguration.friction = controls.friction;
        const auto restoredTargets = longland::dsp::DSPMapper::map (
            physicsEngine.state(), restoredConfiguration);
        consequenceProcessor.setTargets (restoredTargets);
        reflectionProcessor.setTargets (restoredTargets);
        consequenceProcessor.reset();
        reflectionProcessor.reset();
    }

    physicsEngine.setControls (controls);
    physicsEngine.step (static_cast<double> (buffer.getNumSamples()) / currentSampleRate);
    longland::dsp::MappingConfiguration configuration;
    configuration.friction = controls.friction;
    const auto targets = longland::dsp::DSPMapper::map (physicsEngine.state(), configuration);
    consequenceProcessor.setTargets (targets);
    consequenceProcessor.process (buffer);
    reflectionProcessor.setTargets (targets);
    reflectionProcessor.process (buffer);
    snapshotExchange.publish (physicsEngine.snapshot());
}

void LonglandConservationAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destination);
}

void LonglandConservationAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
        // The audio thread performs the actual reset so DSP state and the SPSC
        // snapshot producer never gain a second writer.
        restoreRequested.store (true, std::memory_order_release);
    }
}

juce::AudioProcessorEditor* LonglandConservationAudioProcessor::createEditor()
{
    return new LonglandConservationAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LonglandConservationAudioProcessor();
}
