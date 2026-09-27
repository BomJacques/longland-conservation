#include "../Source/Physics/ConservationEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>

using namespace longland::physics;

namespace
{
int failures = 0;
double maximumObservedConservationError = 0.0;

void check (bool condition, std::string_view message)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void checkConserved (const EnergyState& state, std::string_view context)
{
    maximumObservedConservationError = std::max (
        maximumObservedConservationError, std::abs (state.sum() - state.budget));
    check (state.isFinite(), context);
    check (state.isNonNegative(), context);
    check (std::abs (state.sum() - state.budget) < 1.0e-9, context);
}

double distanceFromEquilibrium (const EnergyState& state)
{
    const auto equilibrium = EnergyState::equilibrium (state.budget);
    double distance = 0.0;
    for (std::size_t i = 0; i < reservoirCount; ++i)
        distance += std::abs (state.amount[i] - equilibrium.amount[i]);
    return distance;
}

void solverConservesAndExhausts()
{
    auto state = EnergyState::equilibrium();
    const auto graph = EnergyTransferGraph::makeDefault();
    ConservationSolver solver;

    EnergyRequest request;
    request.destination = Reservoir::reflected;
    request.requestedAmount = 0.2;
    request.allowGraphFallback = false;
    request.addPreferredDonor (Reservoir::direct);
    const auto result = solver.apply (state, graph, request);
    check (result.fulfilled(), "direct-to-reflected request should be fulfilled");
    check (std::abs (state[Reservoir::direct] - 0.1) < 1.0e-12, "direct must fund reflection");
    check (std::abs (state[Reservoir::reflected] - 0.25) < 1.0e-12, "reflection must receive exact transfer");
    checkConserved (state, "solver transfer must conserve");

    request.requestedAmount = 1.0;
    const auto partial = solver.apply (state, graph, request);
    check (partial.status == TransferStatus::partiallyApplied, "finite donor should limit oversized request");
    check (std::abs (partial.transferredAmount - 0.1) < 1.0e-12, "only remaining direct energy is transferable");
    check (state[Reservoir::direct] >= 0.0, "resource exhaustion must not create negative energy");

    const auto exhausted = solver.apply (state, graph, request);
    check (exhausted.status == TransferStatus::exhausted, "empty donor must report exhaustion");
    check (exhausted.transferredAmount == 0.0, "empty donor must transfer nothing");
    checkConserved (state, "exhausted request must conserve");
}

void solverEnforcesGraphAndRepairsCorruption()
{
    auto state = EnergyState::equilibrium();
    EnergyTransferGraph graph;
    graph.setAllowed (Reservoir::direct, Reservoir::reflected);
    ConservationSolver solver;

    EnergyRequest illegal;
    illegal.destination = Reservoir::reflected;
    illegal.requestedAmount = 0.1;
    illegal.allowGraphFallback = false;
    illegal.addPreferredDonor (Reservoir::noise);
    const auto before = state;
    const auto rejected = solver.apply (state, graph, illegal);
    check (rejected.status == TransferStatus::exhausted, "illegal graph edge must not transfer");
    check (state.amount == before.amount, "illegal request must leave state untouched");

    state[Reservoir::direct] = std::numeric_limits<double>::quiet_NaN();
    const auto repaired = solver.apply (state, graph, illegal);
    check (repaired.status == TransferStatus::invalidState, "corrupt state must be reported");
    check (state.isValid(), "corrupt state must recover to safe equilibrium");
}

void forceDirectionsAreAudibleMappings()
{
    ConservationEngine timeEngine;
    PhysicsControls timeControls;
    timeControls.time = 1.0;
    timeControls.elasticity = 0.0;
    timeControls.friction = 0.35;
    timeEngine.setControls (timeControls);
    for (int i = 0; i < 300; ++i)
        timeEngine.step (0.01);
    check (timeEngine.state()[Reservoir::reflected] > 0.05, "TIME must increase reflected energy");
    check (timeEngine.state()[Reservoir::temporal] > 0.06, "TIME must increase temporal energy");
    check (timeEngine.state()[Reservoir::direct] < 0.30, "TIME must cost direct energy");
    check (timeEngine.state()[Reservoir::transient] < 0.10, "TIME must cost transient energy");
    checkConserved (timeEngine.state(), "TIME force must conserve");

    ConservationEngine pressureEngine;
    PhysicsControls pressure;
    pressure.pressure = 1.0;
    pressure.elasticity = 0.0;
    pressure.friction = 0.35;
    pressureEngine.setControls (pressure);
    for (int i = 0; i < 250; ++i)
        pressureEngine.step (0.01);
    check (pressureEngine.state()[Reservoir::harmonic] > 0.04, "PRESSURE must create harmonic energy");
    check (pressureEngine.state()[Reservoir::mid] > 0.12, "PRESSURE must concentrate mid energy");
    check (pressureEngine.state()[Reservoir::spatial] < 0.05, "PRESSURE must cost spatial energy");
    check (pressureEngine.state()[Reservoir::transient] < 0.10, "PRESSURE must cost transient precision");
    checkConserved (pressureEngine.state(), "PRESSURE force must conserve");
}

void elasticityRestoresAndFrictionDamps()
{
    ConservationEngine displaced;
    PhysicsControls push;
    push.time = 1.0;
    push.elasticity = 0.0;
    push.friction = 0.3;
    displaced.setControls (push);
    for (int i = 0; i < 180; ++i)
        displaced.step (0.01);

    const auto displacedDistance = distanceFromEquilibrium (displaced.state());
    PhysicsControls restore;
    restore.elasticity = 1.0;
    restore.friction = 0.72;
    displaced.setControls (restore);
    for (int i = 0; i < 500; ++i)
        displaced.step (0.01);
    check (distanceFromEquilibrium (displaced.state()) < displacedDistance * 0.35,
           "high elasticity must restore equilibrium, not merely smooth controls");
    checkConserved (displaced.state(), "elastic restoration must conserve");

    ConservationEngine lowFriction;
    ConservationEngine highFriction;
    lowFriction.setControls (push);
    highFriction.setControls (push);
    for (int i = 0; i < 90; ++i)
    {
        lowFriction.step (0.01);
        highFriction.step (0.01);
    }
    const auto lowStart = lowFriction.state()[Reservoir::reflected];
    const auto highStart = highFriction.state()[Reservoir::reflected];

    PhysicsControls coastLow;
    coastLow.elasticity = 0.0;
    coastLow.friction = 0.0;
    auto coastHigh = coastLow;
    coastHigh.friction = 1.0;
    lowFriction.setControls (coastLow);
    highFriction.setControls (coastHigh);
    for (int i = 0; i < 120; ++i)
    {
        lowFriction.step (0.01);
        highFriction.step (0.01);
    }
    const auto lowMotion = lowFriction.state()[Reservoir::reflected] - lowStart;
    const auto highMotion = highFriction.state()[Reservoir::reflected] - highStart;
    check (lowMotion > highMotion * 2.0, "low friction must preserve energy motion longer");
    checkConserved (lowFriction.state(), "low-friction coast must remain bounded");
    checkConserved (highFriction.state(), "high-friction coast must remain bounded");
}

void adversarialAutomationStaysBounded()
{
    ConservationEngine engine;
    engine.prepare (100.0);
    for (int i = 0; i < 1000000; ++i)
    {
        PhysicsControls controls;
        controls.mass = (i % 17) / 16.0;
        controls.time = (i % 2) ? 1.0 : 0.0;
        controls.gravity = (i % 29) / 28.0;
        controls.pressure = (i % 7) / 6.0;
        controls.heat = (i % 11) / 10.0;
        controls.elasticity = (i % 5) / 4.0;
        controls.friction = (i % 3) / 2.0;
        controls.dispersion = (i % 13) / 12.0;
        controls.instability = 1.0;
        controls.energy = 0.2 + 0.8 * ((i % 19) / 18.0);
        engine.setControls (controls);
        engine.step ((i % 4 == 0) ? 0.001 : ((i % 4 == 1) ? 0.01 : ((i % 4 == 2) ? 0.023 : 0.1)));
        checkConserved (engine.state(), "automation torture must conserve every update");
    }

    const auto snapshot = engine.snapshot();
    check (snapshot.valid, "snapshot must report valid after automation torture");
    check (snapshot.activeFlowCount <= maxActiveFlows, "active flow snapshot must stay fixed-capacity");
    for (std::size_t i = 0; i < snapshot.activeFlowCount; ++i)
    {
        check (snapshot.activeFlows[i].ratePerSecond >= 0.0, "visible flow rate must be non-negative");
        check (engine.transferGraph().allows (snapshot.activeFlows[i].source,
                                              snapshot.activeFlows[i].destination),
               "every visible flow must correspond to a legal graph edge");
    }
}

void instabilityIsDeterministicAndControlRateIsStable()
{
    ConservationEngine unstableA;
    ConservationEngine unstableB;
    PhysicsControls unstable;
    unstable.elasticity = 0.5;
    unstable.friction = 0.05;
    unstable.instability = 1.0;
    unstableA.setControls (unstable);
    unstableB.setControls (unstable);
    for (int i = 0; i < 1200; ++i)
    {
        unstableA.step (0.01);
        unstableB.step (0.01);
    }
    check (unstableA.state().amount == unstableB.state().amount,
           "instability must be deterministic physics, not random modulation");
    check (unstableA.snapshot().activeFlowCount > 0,
           "high instability must excite a circulating energy mode");
    checkConserved (unstableA.state(), "instability mode must remain bounded");

    ConservationEngine fine;
    ConservationEngine coarse;
    PhysicsControls force;
    force.mass = 0.65;
    force.time = 0.7;
    force.pressure = 0.35;
    force.elasticity = 0.25;
    force.friction = 0.45;
    fine.setControls (force);
    coarse.setControls (force);
    for (int i = 0; i < 1000; ++i)
        fine.step (0.005);
    for (int i = 0; i < 250; ++i)
        coarse.step (0.02);

    double difference = 0.0;
    for (std::size_t i = 0; i < reservoirCount; ++i)
        difference += std::abs (fine.state().amount[i] - coarse.state().amount[i]);
    check (difference < 0.035, "physics should remain approximately control-rate independent");
    checkConserved (fine.state(), "fine control rate must conserve");
    checkConserved (coarse.state(), "coarse control rate must conserve");
}

void massCreatesInertia()
{
    ConservationEngine light;
    ConservationEngine heavy;
    PhysicsControls lightControls;
    lightControls.time = 1.0;
    lightControls.elasticity = 0.0;
    lightControls.friction = 0.25;
    auto heavyControls = lightControls;
    heavyControls.mass = 1.0;
    light.setControls (lightControls);
    heavy.setControls (heavyControls);
    for (int i = 0; i < 40; ++i)
    {
        light.step (0.01);
        heavy.step (0.01);
    }
    check (heavy.state()[Reservoir::reflected] < light.state()[Reservoir::reflected],
           "high MASS must slow acceleration into the same TIME-driven flow");
    checkConserved (light.state(), "light inertia trajectory must conserve");
    checkConserved (heavy.state(), "heavy inertia trajectory must conserve");
}

void energyBudgetAndInstancesAreIndependent()
{
    ConservationEngine a;
    ConservationEngine b;
    PhysicsControls controls;
    controls.time = 1.0;
    controls.energy = 0.4;
    a.setControls (controls);
    for (int i = 0; i < 100; ++i)
        a.step (0.01);
    check (std::abs (a.state().sum() - 0.4) < 1.0e-10, "ENERGY must scale the conserved budget");
    check (std::abs (b.state().sum() - 1.0) < 1.0e-10, "plugin instances must have independent ledgers");
    check (std::abs (b.state()[Reservoir::direct] - 0.30) < 1.0e-12, "one instance must not move another");

    controls.mass = std::numeric_limits<double>::infinity();
    controls.time = std::numeric_limits<double>::quiet_NaN();
    controls.energy = std::numeric_limits<double>::quiet_NaN();
    a.setControls (controls);
    a.step (std::numeric_limits<double>::quiet_NaN());
    checkConserved (a.state(), "non-finite automation and dt must be sanitised");
}
} // namespace

int main()
{
    solverConservesAndExhausts();
    solverEnforcesGraphAndRepairsCorruption();
    forceDirectionsAreAudibleMappings();
    elasticityRestoresAndFrictionDamps();
    adversarialAutomationStaysBounded();
    instabilityIsDeterministicAndControlRateIsStable();
    massCreatesInertia();
    energyBudgetAndInstancesAreIndependent();

    if (failures != 0)
    {
        std::cerr << failures << " physics assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Longland conservation physics tests passed.\n"
              << "Maximum observed conservation error: " << std::scientific
              << std::setprecision (17) << maximumObservedConservationError << '\n';
    return EXIT_SUCCESS;
}
