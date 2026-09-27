#include "EnergyFieldComponent.h"

#include <algorithm>
#include <cmath>

namespace longland::ui
{
namespace
{
using physics::Reservoir;

constexpr std::array<juce::Point<float>, physics::reservoirCount> nodePositions {
    juce::Point<float> { 0.53f, 0.52f }, // direct/core
    juce::Point<float> { 0.84f, 0.52f }, // reflected
    juce::Point<float> { 0.25f, 0.25f }, // transient
    juce::Point<float> { 0.24f, 0.78f }, // sustain
    juce::Point<float> { 0.08f, 0.52f }, // low
    juce::Point<float> { 0.30f, 0.52f }, // mid
    juce::Point<float> { 0.50f, 0.10f }, // high
    juce::Point<float> { 0.76f, 0.78f }, // harmonic
    juce::Point<float> { 0.93f, 0.25f }, // noise
    juce::Point<float> { 0.75f, 0.25f }, // spatial
    juce::Point<float> { 0.50f, 0.88f }  // temporal (keeps its label inside compact editors)
};

[[nodiscard]] float safeNormalisedAmount (const physics::PhysicsSnapshot& state,
                                          Reservoir reservoir) noexcept
{
    const auto value = state.amounts[physics::indexOf (reservoir)];
    if (! std::isfinite (value))
        return 0.0f;
    return static_cast<float> (std::clamp (value / physics::EnergyState::defaultBudget, 0.0, 1.0));
}

[[nodiscard]] juce::String displayName (Reservoir reservoir)
{
    const auto name = physics::reservoirName (reservoir);
    return juce::String::fromUTF8 (name.data(), static_cast<int> (name.size())).toUpperCase();
}
} // namespace

void EnergyFieldComponent::setSnapshot (const physics::PhysicsSnapshot& newSnapshot)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    snapshot = newSnapshot;
    repaint();
}

std::array<EnergyFieldComponent::NodeGeometry, physics::reservoirCount>
EnergyFieldComponent::makeGeometry (juce::Rectangle<float> bounds) const
{
    std::array<NodeGeometry, physics::reservoirCount> result {};
    const auto scale = std::min (bounds.getWidth(), bounds.getHeight());
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto reservoir = static_cast<Reservoir> (i);
        const auto amount = safeNormalisedAmount (snapshot, reservoir);
        result[i].centre = { bounds.getX() + nodePositions[i].x * bounds.getWidth(),
                             bounds.getY() + nodePositions[i].y * bounds.getHeight() };
        // Filled node area is proportional to the real reservoir amount. Zero
        // energy is represented by the label and flow endpoint, not a fake disc.
        result[i].radius = scale * 0.10f * std::sqrt (amount);
    }
    return result;
}

juce::Colour EnergyFieldComponent::colourFor (Reservoir reservoir) noexcept
{
    switch (reservoir)
    {
        case Reservoir::direct:    return juce::Colour { 0xffe8f5ff };
        case Reservoir::reflected: return juce::Colour { 0xff78c9ff };
        case Reservoir::transient: return juce::Colour { 0xffffc857 };
        case Reservoir::sustain:   return juce::Colour { 0xff82e6a6 };
        case Reservoir::low:       return juce::Colour { 0xffff746c };
        case Reservoir::mid:       return juce::Colour { 0xffffa76c };
        case Reservoir::high:      return juce::Colour { 0xffffec8b };
        case Reservoir::harmonic:  return juce::Colour { 0xffc896ff };
        case Reservoir::noise:     return juce::Colour { 0xffa7b2c3 };
        case Reservoir::spatial:   return juce::Colour { 0xff67eadf };
        case Reservoir::temporal:  return juce::Colour { 0xff889dff };
        case Reservoir::count:     break;
    }
    return juce::Colours::white;
}

void EnergyFieldComponent::paintFlow (
    juce::Graphics& g, const physics::FlowSnapshot& flow,
    const std::array<NodeGeometry, physics::reservoirCount>& geometry) const
{
    const auto sourceIndex = physics::indexOf (flow.source);
    const auto destinationIndex = physics::indexOf (flow.destination);
    if (sourceIndex >= geometry.size() || destinationIndex >= geometry.size()
        || sourceIndex == destinationIndex || ! std::isfinite (flow.ratePerSecond))
        return;

    const auto& source = geometry[sourceIndex];
    const auto& destination = geometry[destinationIndex];
    auto direction = destination.centre - source.centre;
    const auto distance = direction.getDistanceFromOrigin();
    if (distance <= source.radius + destination.radius + 1.0f)
        return;

    direction /= distance;
    const auto start = source.centre + direction * source.radius;
    const auto end = destination.centre - direction * destination.radius;
    const auto magnitude = static_cast<float> (std::clamp (std::abs (flow.ratePerSecond), 0.0, 1.0));
    const auto width = 1.0f + 5.0f * std::sqrt (magnitude);
    const auto colour = colourFor (flow.destination).withAlpha (0.28f + 0.68f * magnitude);

    g.setColour (colour);
    g.drawLine ({ start, end }, width);

    const auto normal = juce::Point<float> { -direction.y, direction.x };
    const auto arrowLength = 7.0f + width;
    const auto arrowWidth = 4.0f + width * 0.7f;
    juce::Path arrow;
    arrow.startNewSubPath (end);
    arrow.lineTo (end - direction * arrowLength + normal * arrowWidth);
    arrow.lineTo (end - direction * arrowLength - normal * arrowWidth);
    arrow.closeSubPath();
    g.fillPath (arrow);
}

void EnergyFieldComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (12.0f, 18.0f);
    g.fillAll (juce::Colour { 0xff080c13 });
    g.setColour (juce::Colour { 0xff182332 });
    g.drawRoundedRectangle (bounds, 12.0f, 1.0f);

    const auto geometry = makeGeometry (bounds.reduced (24.0f, 20.0f));
    const auto flowCount = std::min (snapshot.activeFlowCount, snapshot.activeFlows.size());
    std::array<float, physics::reservoirCount> nodeActivity {};
    for (std::size_t i = 0; i < flowCount; ++i)
    {
        const auto& flow = snapshot.activeFlows[i];
        if (std::isfinite (flow.ratePerSecond))
        {
            const auto activity = static_cast<float> (
                std::clamp (std::abs (flow.ratePerSecond) * 8.0, 0.0, 1.0));
            const auto source = physics::indexOf (flow.source);
            const auto destination = physics::indexOf (flow.destination);
            if (source < nodeActivity.size())
                nodeActivity[source] = std::max (nodeActivity[source], activity);
            if (destination < nodeActivity.size())
                nodeActivity[destination] = std::max (nodeActivity[destination], activity);
        }
        paintFlow (g, snapshot.activeFlows[i], geometry);
    }

    for (std::size_t i = 0; i < geometry.size(); ++i)
    {
        const auto reservoir = static_cast<Reservoir> (i);
        const auto amount = safeNormalisedAmount (snapshot, reservoir);
        const auto activity = nodeActivity[i];
        const auto& node = geometry[i];
        const auto nodeBounds = juce::Rectangle<float> { node.radius * 2.0f, node.radius * 2.0f }
                                    .withCentre (node.centre);
        const auto colour = colourFor (reservoir);

        g.setColour (colour.withAlpha (0.10f + 0.22f * amount + 0.25f * activity));
        g.fillEllipse (nodeBounds.expanded (3.0f + activity * 5.0f));
        g.setColour (colour.withAlpha (0.30f + 0.65f * amount));
        g.fillEllipse (nodeBounds);
        g.setColour (colour.brighter (0.35f).withAlpha (0.8f));
        g.drawEllipse (nodeBounds, 1.0f + activity * 2.0f);

        const auto labelBounds = juce::Rectangle<float> { 110.0f, 18.0f }
                                     .withCentre ({ node.centre.x, nodeBounds.getBottom() + 10.0f });
        g.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
        g.setColour (juce::Colours::white.withAlpha (0.82f));
        g.drawText (displayName (reservoir), labelBounds, juce::Justification::centred);
    }

    if (! snapshot.valid)
    {
        g.setColour (juce::Colour { 0xd9ff4d4d });
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (18.0f).removeFromTop (28.0f), 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
        g.drawText ("INVALID PHYSICS SNAPSHOT", getLocalBounds().removeFromTop (42),
                    juce::Justification::centred);
    }
}
} // namespace longland::ui
