#pragma once

#include "../Physics/ConservationEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace longland::ui
{
class EnergyFieldComponent final : public juce::Component
{
public:
    EnergyFieldComponent() = default;

    // Call on the message thread with a coherent snapshot copied from the
    // processor's snapshot exchange. This component never reaches into DSP state.
    void setSnapshot (const physics::PhysicsSnapshot& newSnapshot);

    void paint (juce::Graphics&) override;

private:
    struct NodeGeometry
    {
        juce::Point<float> centre;
        float radius {};
    };

    [[nodiscard]] std::array<NodeGeometry, physics::reservoirCount>
        makeGeometry (juce::Rectangle<float> bounds) const;
    [[nodiscard]] static juce::Colour colourFor (physics::Reservoir) noexcept;
    void paintFlow (juce::Graphics&, const physics::FlowSnapshot&,
                    const std::array<NodeGeometry, physics::reservoirCount>&) const;

    physics::PhysicsSnapshot snapshot {};
};
} // namespace longland::ui

