#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

#ifndef LONGLAND_VST3_BUNDLE
#error LONGLAND_VST3_BUNDLE must name the built VST3 bundle
#endif

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    const juce::String bundlePath { LONGLAND_VST3_BUNDLE };
    if (! juce::File { bundlePath }.exists())
    {
        std::cerr << "VST3 bundle does not exist: " << bundlePath << '\n';
        return EXIT_FAILURE;
    }

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);
    juce::OwnedArray<juce::PluginDescription> descriptions;
    for (int i = 0; i < formats.getNumFormats(); ++i)
    {
        if (auto* format = formats.getFormat (i); format != nullptr && format->getName() == "VST3")
            format->findAllTypesForFile (descriptions, bundlePath);
    }

    if (descriptions.isEmpty())
    {
        std::cerr << "JUCE VST3 scanner found no plug-in type in the built bundle.\n";
        return EXIT_FAILURE;
    }

    juce::String error;
    auto instance = formats.createPluginInstance (*descriptions[0], 48000.0, 256, error);
    if (instance == nullptr)
    {
        std::cerr << "VST3 instantiation failed: " << error << '\n';
        return EXIT_FAILURE;
    }

    instance->setPlayConfigDetails (2, 2, 48000.0, 256);
    instance->prepareToPlay (48000.0, 256);
    if (instance->getParameters().size() < 10 || instance->getTailLengthSeconds() < 62.0)
    {
        std::cerr << "VST3 did not expose the expected force controls or tail contract.\n";
        return EXIT_FAILURE;
    }

    if (! instance->hasEditor())
    {
        std::cerr << "VST3 reports no editor.\n";
        return EXIT_FAILURE;
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor { instance->createEditorIfNeeded() };
    if (editor == nullptr || editor->getWidth() < 760 || editor->getHeight() < 620)
    {
        std::cerr << "VST3 editor creation or initial layout failed.\n";
        return EXIT_FAILURE;
    }
    editor.reset();
    juce::AudioBuffer<float> audio (2, 256);
    juce::MidiBuffer midi;
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        const auto value = sample == 0 ? 1.0f : 0.0f;
        audio.setSample (0, sample, value);
        audio.setSample (1, sample, value);
    }
    instance->processBlock (audio, midi);
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            const auto value = audio.getSample (channel, sample);
            if (! std::isfinite (value) || std::abs (value) > 0.8912509f + 1.0e-5f)
            {
                std::cerr << "VST3 render produced an invalid or unbounded sample.\n";
                return EXIT_FAILURE;
            }
        }
    }

    instance->releaseResources();
    std::cout << "Built VST3 scanned, instantiated, and rendered successfully.\n";
    return EXIT_SUCCESS;
}
