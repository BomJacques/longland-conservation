#include "PhysicsDebugPanel.h"

#include <algorithm>
#include <cmath>

namespace longland::ui
{
namespace
{
[[nodiscard]] juce::String nameOf (physics::Reservoir reservoir)
{
    const auto name = physics::reservoirName (reservoir);
    return juce::String::fromUTF8 (name.data(), static_cast<int> (name.size())).toUpperCase();
}

[[nodiscard]] juce::String fixed (double value, int decimals)
{
    return std::isfinite (value) ? juce::String { value, decimals } : juce::String { "NON-FINITE" };
}
} // namespace

void PhysicsDebugPanel::setSnapshot (const physics::PhysicsSnapshot& newSnapshot)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    snapshot = newSnapshot;
    repaint();
}

void PhysicsDebugPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour { 0xff0b1018 });
    auto area = getLocalBounds().reduced (12);

    g.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawText ("DEBUG PHYSICS", area.removeFromTop (22), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions { juce::Font::getDefaultMonospacedFontName(), 12.0f,
                                  juce::Font::plain });
    const auto rowHeight = 18;
    const auto drawRow = [&] (juce::String label, juce::String value, juce::Colour colour) {
        auto row = area.removeFromTop (rowHeight);
        auto left = row.removeFromLeft (std::max (100, getWidth() / 2));
        g.setColour (juce::Colours::white.withAlpha (0.68f));
        g.drawText (label, left, juce::Justification::centredLeft);
        g.setColour (colour);
        g.drawText (value, row, juce::Justification::centredRight);
    };

    drawRow ("TOTAL ENERGY", fixed (snapshot.budget, 6), juce::Colours::white);
    drawRow ("STATE SUM", fixed (snapshot.stateSum, 6), juce::Colours::white);
    const auto errorBad = ! std::isfinite (snapshot.error)
                       || std::abs (snapshot.error) > physics::EnergyState::conservationTolerance;
    drawRow ("ERROR", fixed (snapshot.error, 9),
             errorBad ? juce::Colour { 0xffff6262 } : juce::Colour { 0xff76e6a0 });
    drawRow ("VALID / SEQUENCE",
             juce::String { snapshot.valid ? "YES / " : "NO / " }
                 + juce::String { static_cast<juce::int64> (snapshot.sequence) },
             snapshot.valid ? juce::Colour { 0xff76e6a0 } : juce::Colour { 0xffff6262 });

    area.removeFromTop (5);
    for (std::size_t i = 0; i < physics::reservoirCount; ++i)
    {
        const auto reservoir = static_cast<physics::Reservoir> (i);
        drawRow (nameOf (reservoir), fixed (snapshot.amounts[i], 6), juce::Colours::white);
    }

    area.removeFromTop (8);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions { 11.0f, juce::Font::bold });
    g.drawText ("ACTIVE TRANSFERS", area.removeFromTop (rowHeight), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions { juce::Font::getDefaultMonospacedFontName(), 11.0f,
                                  juce::Font::plain });

    const auto count = std::min (snapshot.activeFlowCount, snapshot.activeFlows.size());
    if (count == 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.38f));
        g.drawText ("NONE", area.removeFromTop (rowHeight), juce::Justification::centredLeft);
    }
    else
    {
        for (std::size_t i = 0; i < count && area.getHeight() >= rowHeight; ++i)
        {
            const auto& flow = snapshot.activeFlows[i];
            const auto line = nameOf (flow.source) + "  ->  " + nameOf (flow.destination);
            const auto rate = juce::String { flow.ratePerSecond >= 0.0 ? "+" : "" }
                            + fixed (flow.ratePerSecond, 6) + " /s";
            auto row = area.removeFromTop (rowHeight);
            auto left = row.removeFromLeft (std::max (130, getWidth() * 2 / 3));
            g.setColour (juce::Colours::white.withAlpha (0.70f));
            g.drawText (line, left, juce::Justification::centredLeft);
            g.setColour (juce::Colour { 0xff78c9ff });
            g.drawText (rate, row, juce::Justification::centredRight);
        }
    }
}
} // namespace longland::ui
