#pragma once

#include "ConservationSolver.h"

#include <array>
#include <cstdint>

namespace longland::physics
{
struct PhysicsControls
{
    double mass {};
    double time {};
    double gravity {};
    double pressure {};
    double heat {};
    double elasticity { 0.35 };
    double friction { 0.55 };
    double dispersion {};
    double instability {};
    double energy { 1.0 };
};

struct FlowSnapshot
{
    Reservoir source { Reservoir::direct };
    Reservoir destination { Reservoir::direct };
    double ratePerSecond {};
};

inline constexpr std::size_t maxActiveFlows = 24;

struct PhysicsSnapshot
{
    ReservoirValues amounts {};
    std::array<FlowSnapshot, maxActiveFlows> activeFlows {};
    std::size_t activeFlowCount {};
    double budget {};
    double stateSum {};
    double error {};
    bool valid {};
    std::uint64_t sequence {};
};

class ConservationEngine
{
public:
    ConservationEngine() noexcept;

    void prepare (double controlRateHz) noexcept;
    void reset (double budget = EnergyState::defaultBudget) noexcept;
    void setControls (const PhysicsControls& newControls) noexcept;
    void step (double deltaSeconds) noexcept;

    [[nodiscard]] const PhysicsControls& controls() const noexcept { return currentControls; }
    [[nodiscard]] const EnergyState& state() const noexcept { return currentState; }
    [[nodiscard]] const EnergyTransferGraph& transferGraph() const noexcept { return graph; }
    [[nodiscard]] PhysicsSnapshot snapshot() const noexcept;

private:
    using FlowMatrix = std::array<std::array<double, reservoirCount>, reservoirCount>;

    void advanceOneStep (double deltaSeconds) noexcept;
    void accelerate (Reservoir source, Reservoir destination, double acceleration,
                     double deltaSeconds) noexcept;
    void applyUserForces (double deltaSeconds) noexcept;
    void applyElasticForce (double deltaSeconds) noexcept;
    void applyInstabilityMode (double deltaSeconds) noexcept;
    void cancelOpposingFlows() noexcept;
    void captureActiveFlows (double outerDeltaSeconds) noexcept;

    EnergyState currentState { EnergyState::equilibrium() };
    EnergyState equilibriumState { EnergyState::equilibrium() };
    EnergyTransferGraph graph { EnergyTransferGraph::makeDefault() };
    ConservationSolver solver;
    PhysicsControls currentControls;
    FlowMatrix velocity {};
    FlowMatrix transferredThisStep {};
    std::array<FlowSnapshot, maxActiveFlows> visibleFlows {};
    std::size_t visibleFlowCount {};
    double preparedDeltaSeconds { 1.0 / 100.0 };
    double instabilityPhase {};
    std::uint64_t stateSequence {};
};
} // namespace longland::physics
