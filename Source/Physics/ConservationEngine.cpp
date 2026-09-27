#include "ConservationEngine.h"

#include <algorithm>
#include <cmath>

namespace longland::physics
{
namespace
{
constexpr double pi = 3.14159265358979323846;

[[nodiscard]] double unitValue (double value, double fallback = 0.0) noexcept
{
    return std::isfinite (value) ? std::clamp (value, 0.0, 1.0) : fallback;
}

[[nodiscard]] PhysicsControls sanitised (PhysicsControls controls) noexcept
{
    controls.mass = unitValue (controls.mass);
    controls.time = unitValue (controls.time);
    controls.gravity = unitValue (controls.gravity);
    controls.pressure = unitValue (controls.pressure);
    controls.heat = unitValue (controls.heat);
    controls.elasticity = unitValue (controls.elasticity, 0.35);
    controls.friction = unitValue (controls.friction, 0.55);
    controls.dispersion = unitValue (controls.dispersion);
    controls.instability = unitValue (controls.instability);
    controls.energy = unitValue (controls.energy, 1.0);
    return controls;
}
} // namespace

ConservationEngine::ConservationEngine() noexcept
{
    reset();
}

void ConservationEngine::prepare (double controlRateHz) noexcept
{
    if (std::isfinite (controlRateHz) && controlRateHz >= 10.0 && controlRateHz <= 2000.0)
        preparedDeltaSeconds = 1.0 / controlRateHz;
}

void ConservationEngine::reset (double budget) noexcept
{
    currentState = EnergyState::equilibrium (budget);
    equilibriumState = currentState;
    for (auto& row : velocity)
        row.fill (0.0);
    for (auto& row : transferredThisStep)
        row.fill (0.0);
    visibleFlows.fill ({});
    visibleFlowCount = 0;
    instabilityPhase = 0.0;
    ++stateSequence;
}

void ConservationEngine::setControls (const PhysicsControls& newControls) noexcept
{
    currentControls = sanitised (newControls);
}

void ConservationEngine::accelerate (Reservoir source, Reservoir destination, double acceleration,
                                      double deltaSeconds) noexcept
{
    if (! graph.allows (source, destination) || ! std::isfinite (acceleration) || acceleration <= 0.0)
        return;

    auto& speed = velocity[indexOf (source)][indexOf (destination)];
    const auto maximumSpeed = std::max (1.0e-9, currentState.budget * 1.5);
    // MASS creates real inertia for every flow: the same force changes velocity
    // more slowly, while the authored MASS forces still establish the heavy
    // low/sustain equilibrium preference.
    const auto inertiaScale = 1.0 / (1.0 + 3.0 * currentControls.mass * currentControls.mass);
    speed = std::clamp (speed + acceleration * deltaSeconds * inertiaScale, 0.0, maximumSpeed);
}

void ConservationEngine::applyUserForces (double dt) noexcept
{
    const auto budget = currentState.budget;
    const auto push = [&] (Reservoir source, Reservoir destination, double control, double coefficient)
    {
        accelerate (source, destination, budget * control * coefficient, dt);
    };

    push (Reservoir::high, Reservoir::low, currentControls.mass, 0.18);
    push (Reservoir::transient, Reservoir::sustain, currentControls.mass, 0.13);
    push (Reservoir::spatial, Reservoir::low, currentControls.mass, 0.06);

    push (Reservoir::direct, Reservoir::reflected, currentControls.time, 0.22);
    push (Reservoir::transient, Reservoir::temporal, currentControls.time, 0.16);
    push (Reservoir::direct, Reservoir::temporal, currentControls.time, 0.08);

    push (Reservoir::high, Reservoir::low, currentControls.gravity, 0.16);
    push (Reservoir::spatial, Reservoir::direct, currentControls.gravity, 0.13);
    push (Reservoir::transient, Reservoir::sustain, currentControls.gravity, 0.06);

    push (Reservoir::spatial, Reservoir::harmonic, currentControls.pressure, 0.16);
    push (Reservoir::transient, Reservoir::mid, currentControls.pressure, 0.13);
    push (Reservoir::direct, Reservoir::harmonic, currentControls.pressure, 0.09);

    push (Reservoir::direct, Reservoir::harmonic, currentControls.heat, 0.13);
    push (Reservoir::transient, Reservoir::noise, currentControls.heat, 0.11);
    push (Reservoir::low, Reservoir::high, currentControls.heat, 0.09);

    push (Reservoir::direct, Reservoir::spatial, currentControls.dispersion, 0.20);
    push (Reservoir::transient, Reservoir::reflected, currentControls.dispersion, 0.09);
    push (Reservoir::mid, Reservoir::high, currentControls.dispersion, 0.06);
}

void ConservationEngine::applyElasticForce (double dt) noexcept
{
    if (currentControls.elasticity <= 0.0 || currentState.budget <= 0.0)
        return;

    const auto stiffness = 8.0 * currentControls.elasticity * (1.0 + 0.7 * currentControls.instability);
    for (std::size_t destinationIndex = 0; destinationIndex < reservoirCount; ++destinationIndex)
    {
        const auto destination = static_cast<Reservoir> (destinationIndex);
        const auto deficit = equilibriumState[destination] - currentState[destination];
        if (deficit <= 0.0)
            continue;

        double legalSurplus = 0.0;
        for (std::size_t sourceIndex = 0; sourceIndex < reservoirCount; ++sourceIndex)
        {
            const auto source = static_cast<Reservoir> (sourceIndex);
            if (graph.allows (source, destination))
                legalSurplus += std::max (0.0, currentState[source] - equilibriumState[source]);
        }

        if (legalSurplus <= 0.0)
            continue;

        for (std::size_t sourceIndex = 0; sourceIndex < reservoirCount; ++sourceIndex)
        {
            const auto source = static_cast<Reservoir> (sourceIndex);
            const auto surplus = std::max (0.0, currentState[source] - equilibriumState[source]);
            if (surplus > 0.0 && graph.allows (source, destination))
                accelerate (source, destination, stiffness * std::min (deficit, legalSurplus)
                                                        * surplus / legalSurplus, dt);
        }
    }
}

void ConservationEngine::applyInstabilityMode (double dt) noexcept
{
    const auto amount = currentControls.instability;
    if (amount <= 0.0 || currentState.budget <= 0.0)
        return;

    // Instability excites a deterministic circulating mode in the legal graph.
    // It changes the physics (coupling and damping), rather than adding random
    // parameter modulation. The sign reverses the authored cycle each half-wave.
    constexpr std::array cycle {
        Reservoir::direct, Reservoir::reflected, Reservoir::transient, Reservoir::mid,
        Reservoir::harmonic, Reservoir::high, Reservoir::low, Reservoir::sustain,
        Reservoir::temporal, Reservoir::direct
    };
    instabilityPhase += 2.0 * pi * (0.12 + 0.88 * amount) * dt;
    if (instabilityPhase > 2.0 * pi)
        instabilityPhase = std::fmod (instabilityPhase, 2.0 * pi);

    const auto wave = std::sin (instabilityPhase);
    const auto acceleration = currentState.budget * amount * amount * 0.11 * std::abs (wave);
    for (std::size_t i = 0; i + 1 < cycle.size(); ++i)
    {
        const auto source = wave >= 0.0 ? cycle[i] : cycle[i + 1];
        const auto destination = wave >= 0.0 ? cycle[i + 1] : cycle[i];
        accelerate (source, destination, acceleration, dt);
    }
}

void ConservationEngine::cancelOpposingFlows() noexcept
{
    for (std::size_t a = 0; a < reservoirCount; ++a)
    {
        for (std::size_t b = a + 1; b < reservoirCount; ++b)
        {
            const auto cancelled = std::min (velocity[a][b], velocity[b][a]);
            velocity[a][b] -= cancelled;
            velocity[b][a] -= cancelled;
        }
    }
}

void ConservationEngine::advanceOneStep (double dt) noexcept
{
    const auto effectiveDamping = (0.10 + 9.0 * currentControls.friction)
                                  * (1.0 - 0.78 * currentControls.instability);
    const auto damping = std::exp (-effectiveDamping * dt);
    for (auto& row : velocity)
        for (auto& speed : row)
            speed *= damping;

    applyUserForces (dt);
    applyElasticForce (dt);
    applyInstabilityMode (dt);
    cancelOpposingFlows();

    // Resolve destinations competing for the same donor proportionally from a
    // coherent pre-transfer state. Incoming energy becomes spendable on the next
    // substep, eliminating enum-order starvation within this step.
    std::array<double, reservoirCount> sourceScale {};
    for (std::size_t sourceIndex = 0; sourceIndex < reservoirCount; ++sourceIndex)
    {
        double demand = 0.0;
        for (std::size_t destinationIndex = 0; destinationIndex < reservoirCount; ++destinationIndex)
            demand += velocity[sourceIndex][destinationIndex] * dt;
        sourceScale[sourceIndex] = demand > 0.0
            ? std::min (1.0, currentState.amount[sourceIndex] / demand) : 1.0;
    }

    for (std::size_t sourceIndex = 0; sourceIndex < reservoirCount; ++sourceIndex)
    {
        for (std::size_t destinationIndex = 0; destinationIndex < reservoirCount; ++destinationIndex)
        {
            auto& speed = velocity[sourceIndex][destinationIndex];
            if (speed <= 1.0e-12)
                continue;

            EnergyRequest request;
            request.destination = static_cast<Reservoir> (destinationIndex);
            request.requestedAmount = speed * dt * sourceScale[sourceIndex];
            request.allowGraphFallback = false;
            request.addPreferredDonor (static_cast<Reservoir> (sourceIndex));
            const auto result = solver.apply (currentState, graph, request);
            transferredThisStep[sourceIndex][destinationIndex] += result.transferredAmount;

            if (! result.fulfilled())
                speed *= result.transferredAmount > 0.0 ? 0.35 : 0.08;
        }
    }

    if (! currentState.isValid())
    {
        currentState.validateAndRenormalise();
        for (auto& row : velocity)
            for (auto& speed : row)
                speed = std::isfinite (speed) ? std::clamp (speed, 0.0, currentState.budget) : 0.0;
    }
}

void ConservationEngine::captureActiveFlows (double outerDt) noexcept
{
    visibleFlowCount = 0;
    visibleFlows.fill ({});
    if (outerDt <= 0.0)
        return;

    for (std::size_t source = 0; source < reservoirCount; ++source)
    {
        for (std::size_t destination = 0; destination < reservoirCount; ++destination)
        {
            const auto rate = transferredThisStep[source][destination] / outerDt;
            if (rate <= 1.0e-10)
                continue;

            FlowSnapshot candidate { static_cast<Reservoir> (source),
                                     static_cast<Reservoir> (destination), rate };
            if (visibleFlowCount < visibleFlows.size())
            {
                visibleFlows[visibleFlowCount++] = candidate;
                continue;
            }

            auto smallest = std::min_element (visibleFlows.begin(), visibleFlows.end(),
                [] (const auto& a, const auto& b) { return a.ratePerSecond < b.ratePerSecond; });
            if (candidate.ratePerSecond > smallest->ratePerSecond)
                *smallest = candidate;
        }
    }

    std::sort (visibleFlows.begin(), visibleFlows.begin() + static_cast<std::ptrdiff_t> (visibleFlowCount),
               [] (const auto& a, const auto& b) { return a.ratePerSecond > b.ratePerSecond; });
}

void ConservationEngine::step (double deltaSeconds) noexcept
{
    const auto requestedDelta = std::isfinite (deltaSeconds) && deltaSeconds > 0.0
                              ? std::clamp (deltaSeconds, 1.0e-8, 0.25)
                              : preparedDeltaSeconds;

    currentState.setBudget (currentControls.energy);
    equilibriumState = EnergyState::equilibrium (currentState.budget);
    for (auto& row : transferredThisStep)
        row.fill (0.0);

    constexpr double maximumInternalDelta = 1.0 / 240.0;
    const auto iterations = std::clamp (static_cast<int> (std::ceil (requestedDelta / maximumInternalDelta)), 1, 64);
    const auto internalDelta = requestedDelta / static_cast<double> (iterations);
    for (int i = 0; i < iterations; ++i)
        advanceOneStep (internalDelta);

    captureActiveFlows (requestedDelta);
    ++stateSequence;
}

PhysicsSnapshot ConservationEngine::snapshot() const noexcept
{
    PhysicsSnapshot result;
    result.amounts = currentState.amount;
    result.activeFlows = visibleFlows;
    result.activeFlowCount = visibleFlowCount;
    result.budget = currentState.budget;
    result.stateSum = currentState.sum();
    result.error = currentState.conservationError();
    result.valid = currentState.isValid();
    result.sequence = stateSequence;
    return result;
}
} // namespace longland::physics
