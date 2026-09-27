#pragma once

#include "../Physics/ConservationEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace longland::ui
{
class PhysicsDebugPanel final : public juce::Component
{
public:
    PhysicsDebugPanel() = default;

    // Snapshot acquisition belongs to the editor/processor bridge. Keeping this a
    // value copy makes it impossible for paint() to observe a half-updated ledger.
    void setSnapshot (const physics::PhysicsSnapshot& newSnapshot);
    void paint (juce::Graphics&) override;

private:
    physics::PhysicsSnapshot snapshot {};
};
} // namespace longland::ui

