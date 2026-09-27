#include "../Source/PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

#ifndef LONGLAND_UI_SNAPSHOT
#error LONGLAND_UI_SNAPSHOT must name the UI render artifact
#endif

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

void setParameter (LonglandConservationAudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.parameters.getParameter (id))
        parameter->setValueNotifyingHost (std::clamp (value, 0.0f, 1.0f));
    else
        check (false, "required parameter must exist");
}

std::vector<float> processProgram (LonglandConservationAudioProcessor& processor,
                                   double sampleRate, int blockSize, int blocks,
                                   bool impulseFirst = false)
{
    processor.prepareToPlay (sampleRate, blockSize);
    std::vector<float> result;
    result.reserve (static_cast<std::size_t> (blockSize * blocks * 2));
    juce::MidiBuffer midi;
    double phase = 0.0;
    const auto increment = 2.0 * 3.14159265358979323846 * 997.0 / sampleRate;

    for (int blockIndex = 0; blockIndex < blocks; ++blockIndex)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            auto value = 0.18f * static_cast<float> (std::sin (phase));
            if (impulseFirst && blockIndex == 0 && sample == 0)
                value = 1.0f;
            phase += increment;
            buffer.setSample (0, sample, value);
            buffer.setSample (1, sample, value * 0.87f);
        }
        processor.processBlock (buffer, midi);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto value = buffer.getSample (channel, sample);
                check (std::isfinite (value), "actual plugin output must remain finite");
                check (std::abs (value) <= 0.8912509f + 1.0e-5f,
                       "actual plugin output must obey the sample ceiling");
                result.push_back (value);
            }
        }
    }
    return result;
}

void actualProcessorSurvivesHostMatrix()
{
    for (const auto sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        for (const auto blockSize : { 1, 7, 31, 64, 127, 256, 511, 1024 })
        {
            LonglandConservationAudioProcessor processor;
            setParameter (processor, "time", 1.0f);
            setParameter (processor, "pressure", 0.85f);
            setParameter (processor, "heat", 0.75f);
            setParameter (processor, "dispersion", 0.9f);
            setParameter (processor, "instability", 1.0f);
            setParameter (processor, "friction", 0.05f);
            processProgram (processor, sampleRate, blockSize, 12, true);
            const auto snapshot = processor.consumePhysicsSnapshot();
            check (snapshot.valid, "actual plugin snapshot must remain valid across host matrix");
            check (std::abs (snapshot.stateSum - snapshot.budget) < 1.0e-9,
                   "actual plugin must conserve its ledger across host matrix");

            juce::AudioBuffer<float> empty (2, 0);
            juce::MidiBuffer midi;
            processor.processBlock (empty, midi);
        }
    }
}

void stateRecallIsDeterministic()
{
    LonglandConservationAudioProcessor source;
    setParameter (source, "mass", 0.43f);
    setParameter (source, "time", 0.78f);
    setParameter (source, "pressure", 0.62f);
    setParameter (source, "elasticity", 0.71f);
    setParameter (source, "friction", 0.29f);
    setParameter (source, "energy", 0.83f);
    juce::MemoryBlock saved;
    source.getStateInformation (saved);

    LonglandConservationAudioProcessor a;
    LonglandConservationAudioProcessor b;
    setParameter (a, "heat", 1.0f);
    setParameter (a, "instability", 1.0f);
    setParameter (b, "gravity", 1.0f);
    setParameter (b, "dispersion", 1.0f);
    processProgram (a, 48000.0, 127, 80, true);
    processProgram (b, 48000.0, 127, 11, true);

    a.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    b.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    const auto outputA = processProgram (a, 48000.0, 127, 40, true);
    const auto outputB = processProgram (b, 48000.0, 127, 40, true);
    check (outputA.size() == outputB.size(), "restored renders must have matching sizes");
    double maximumDifference = 0.0;
    for (std::size_t i = 0; i < std::min (outputA.size(), outputB.size()); ++i)
        maximumDifference = std::max (maximumDifference,
                                      std::abs (static_cast<double> (outputA[i] - outputB[i])));
    check (maximumDifference < 1.0e-7,
           "restoring identical state must erase prior DSP/physics history deterministically");
}

void instancesRemainIndependent()
{
    LonglandConservationAudioProcessor moving;
    LonglandConservationAudioProcessor neutral;
    setParameter (moving, "time", 1.0f);
    setParameter (moving, "instability", 1.0f);
    processProgram (moving, 48000.0, 64, 200);
    processProgram (neutral, 48000.0, 64, 1);
    const auto a = moving.consumePhysicsSnapshot();
    const auto b = neutral.consumePhysicsSnapshot();
    check (a.amounts != b.amounts, "plugin instances must not share mutable physics state");
    check (moving.getTailLengthSeconds() >= 62.0,
           "declared host tail must cover maximum low-friction delay decay");
}

void editorRendersRealContent()
{
    LonglandConservationAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor { processor.createEditor() };
    check (editor != nullptr && editor->getWidth() >= 760 && editor->getHeight() >= 620,
           "direct plugin editor must open at a usable size");
    if (editor == nullptr)
        return;

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true);
    bool foundVisiblePixel = false;
    for (int y = 0; y < image.getHeight() && ! foundVisiblePixel; y += 8)
        for (int x = 0; x < image.getWidth(); x += 8)
            foundVisiblePixel = foundVisiblePixel || image.getPixelAt (x, y).getBrightness() > 0.04f;
    check (image.isValid() && foundVisiblePixel,
           "editor snapshot must contain real controls/visualizer pixels, not a black surface");

    juce::File snapshotFile { juce::String { LONGLAND_UI_SNAPSHOT } };
    snapshotFile.deleteFile();
    if (auto stream = snapshotFile.createOutputStream(); stream != nullptr)
    {
        juce::PNGImageFormat png;
        check (png.writeImageToStream (image, *stream), "editor PNG encoding must succeed");
    }
    else
    {
        check (false, "editor snapshot file must be writable");
    }
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    actualProcessorSurvivesHostMatrix();
    stateRecallIsDeterministic();
    instancesRemainIndependent();
    editorRendersRealContent();

    if (failures != 0)
    {
        std::cerr << failures << " plugin smoke assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Longland plugin smoke tests passed.\n";
    return EXIT_SUCCESS;
}
