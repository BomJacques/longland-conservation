#include "../Source/DSP/DSPMapper.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string_view>

using namespace longland::dsp;
using namespace longland::physics;

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

void delayImpulsePowerIsConserved()
{
    constexpr double reflectedPower = 0.63;
    constexpr double delayShare = 0.75;
    constexpr double feedback = 0.80;
    const auto gains = DSPMapper::makeConservedDelayGains (
        reflectedPower, delayShare, feedback);

    const auto dryEnergy = gains.direct * gains.direct;
    const auto wetTailEnergy = gains.delayInput * gains.delayInput
        / (1.0 - gains.feedback * gains.feedback);
    check (std::abs (dryEnergy - (1.0 - reflectedPower)) < 1.0e-12,
           "dry impulse energy must equal unreflected power");
    check (std::abs (wetTailEnergy - reflectedPower * delayShare) < 1.0e-12,
           "geometric delay tail must consume exactly its reflected partition");

    const auto longerTail = DSPMapper::makeConservedDelayGains (
        reflectedPower, delayShare, 0.90);
    const auto longerTailEnergy = longerTail.delayInput * longerTail.delayInput
        / (1.0 - longerTail.feedback * longerTail.feedback);
    check (longerTail.delayInput < gains.delayInput,
           "more feedback must reduce the first echo instead of creating power");
    check (std::abs (longerTailEnergy - wetTailEnergy) < 1.0e-12,
           "feedback must redistribute fixed tail power across time");
}

void directToReflectedTransferIsAudibleInTargets()
{
    const auto neutral = EnergyState::equilibrium();
    auto displaced = neutral;
    displaced[Reservoir::direct] -= 0.20;
    displaced[Reservoir::reflected] += 0.20;
    check (displaced.isValid(), "test transfer must preserve the ledger");

    const auto before = DSPMapper::map (neutral);
    const auto after = DSPMapper::map (displaced);
    check (after.directPower < before.directPower,
           "reflected gain must make direct power recede");
    check (after.delayPower + after.reverbPower > before.delayPower + before.reverbPower,
           "reflected gain must increase the total wet partition");
    check (after.reflection.direct < before.reflection.direct,
           "audible dry coefficient must decrease monotonically");
    check (std::abs (after.directPower + after.delayPower + after.reverbPower - 1.0) < 1.0e-12,
           "direct, delay, and reverb partitions must sum to one");
}

void opposingReservoirsProduceAttenuationOnlyCosts()
{
    auto transientState = EnergyState::equilibrium();
    transientState[Reservoir::transient] += 0.08;
    transientState[Reservoir::sustain] -= 0.08;
    const auto transientTargets = DSPMapper::map (transientState);
    check (transientTargets.transientSegmentGain == 1.0,
           "transient preference must not boost attacks");
    check (transientTargets.sustainSegmentGain < 1.0,
           "transient preference must be paid for by sustain attenuation");

    auto sustainState = EnergyState::equilibrium();
    sustainState[Reservoir::sustain] += 0.08;
    sustainState[Reservoir::transient] -= 0.08;
    const auto sustainTargets = DSPMapper::map (sustainState);
    check (sustainTargets.sustainSegmentGain == 1.0,
           "sustain preference must not boost held material");
    check (sustainTargets.transientSegmentGain < 1.0,
           "sustain preference must be paid for by attack attenuation");

    auto brightState = EnergyState::equilibrium();
    brightState[Reservoir::high] += 0.07;
    brightState[Reservoir::low] -= 0.07;
    const auto brightTargets = DSPMapper::map (brightState);
    check (brightTargets.spectralBandGains[2] == 1.0,
           "dominant high band must remain at unity rather than boost");
    check (brightTargets.spectralBandGains[0] < 1.0,
           "brightness must cost low-band level");

    auto pressureState = EnergyState::equilibrium();
    pressureState[Reservoir::harmonic] += 0.03;
    pressureState[Reservoir::direct] -= 0.03;
    const auto pressureTargets = DSPMapper::map (pressureState);
    check (pressureTargets.harmonicBlend > 0.0 && pressureTargets.harmonicDrive > 1.0,
           "harmonic displacement must engage the bounded shaper");

    auto wideState = EnergyState::equilibrium();
    wideState[Reservoir::spatial] += 0.10;
    wideState[Reservoir::direct] -= 0.10;
    const auto wideTargets = DSPMapper::map (wideState);
    check (wideTargets.sideGain == 1.0 && wideTargets.midGain < 1.0,
           "width must come from centre loss, not side gain");
}

void corruptAndExtremeStatesMapSafely()
{
    auto silent = EnergyState::equilibrium (0.0);
    const auto silentTargets = DSPMapper::map (silent);
    check (silentTargets.outputGain == 0.0 && silentTargets.directPower == 0.0,
           "zero global budget must produce zero output target");
    check (silentTargets.isFiniteAndBounded(), "zero-budget targets must remain valid");

    auto corrupt = EnergyState::equilibrium();
    corrupt[Reservoir::direct] = std::numeric_limits<double>::quiet_NaN();
    corrupt[Reservoir::reflected] = std::numeric_limits<double>::infinity();
    check (DSPMapper::map (corrupt).isFiniteAndBounded(),
           "non-finite ledger values must recover to bounded targets");

    MappingConfiguration corruptConfiguration;
    corruptConfiguration.friction = std::numeric_limits<double>::quiet_NaN();
    corruptConfiguration.maximumFeedback = std::numeric_limits<double>::infinity();
    corruptConfiguration.minimumDelaySeconds = -std::numeric_limits<double>::infinity();
    check (DSPMapper::map (EnergyState::equilibrium(), corruptConfiguration).isFiniteAndBounded(),
           "non-finite mapper configuration must recover safely");

    std::mt19937_64 random { 0x10A61A7DULL };
    std::uniform_real_distribution<double> distribution (0.0, 1.0);
    for (int iteration = 0; iteration < 100000; ++iteration)
    {
        EnergyState state;
        for (auto& value : state.amount)
            value = distribution (random);
        state.budget = distribution (random);
        state.validateAndRenormalise();
        check (DSPMapper::map (state).isFiniteAndBounded(),
               "all valid randomized ledgers must map to bounded targets");
    }
}

void depletedAudiblePairCannotRenormaliseToFullPower()
{
    const auto neutralTargets = DSPMapper::map (EnergyState::equilibrium());
    auto migrated = EnergyState::equilibrium();
    migrated[Reservoir::direct] -= 0.25;
    migrated[Reservoir::harmonic] += 0.25;
    const auto migratedTargets = DSPMapper::map (migrated);
    check (migratedTargets.outputGain < neutralTargets.outputGain,
           "energy migrated out of Direct/Reflected must audibly reduce that subsystem");
    check (migratedTargets.outputGain <= 1.0,
           "conditional subsystem mapping must never restore a depleted pair by boosting");
}
} // namespace

int main()
{
    delayImpulsePowerIsConserved();
    directToReflectedTransferIsAudibleInTargets();
    opposingReservoirsProduceAttenuationOnlyCosts();
    corruptAndExtremeStatesMapSafely();
    depletedAudiblePairCannotRenormaliseToFullPower();

    if (failures != 0)
    {
        std::cerr << failures << " DSP mapper assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Longland DSP mapper tests passed.\n";
    return EXIT_SUCCESS;
}
