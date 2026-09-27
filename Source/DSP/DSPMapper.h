#pragma once

#include "../Physics/EnergyState.h"

#include <array>

namespace longland::dsp
{
struct MappingConfiguration
{
    // Friction is a property of the physics system, not an independent effect
    // control. The engine should publish its current effective value with the
    // ledger snapshot.
    double friction { 0.5 };
    double maximumFeedback { 0.92 };
    double minimumDelaySeconds { 0.030 };
    double maximumDelaySeconds { 0.750 };
    double minimumReverbSeconds { 0.20 };
    double maximumReverbSeconds { 8.0 };
};

struct ReflectionPowerGains
{
    double direct { 1.0 };
    double delayInput { 0.0 };
    double feedback { 0.0 };
};

struct DSPTargets
{
    double outputGain { 1.0 };

    double directPower { 1.0 };
    double delayPower { 0.0 };
    double reverbPower { 0.0 };
    ReflectionPowerGains reflection {};
    double reverbInputGain { 0.0 };
    double delaySeconds { 0.125 };
    double reverbT60Seconds { 0.5 };
    double reflectionLowpassHz { 8000.0 };

    // A transient detector supplies a mask in [0, 1]. The processor should
    // interpolate these two gains using that mask; neither target boosts.
    double transientSegmentGain { 1.0 };
    double sustainSegmentGain { 1.0 };

    std::array<double, 3> spectralBandGains { 1.0, 1.0, 1.0 };
    double harmonicBlend { 0.0 };
    double harmonicDrive { 1.0 };
    double noiseTextureBlend { 0.0 };

    double midGain { 1.0 };
    double sideGain { 1.0 };

    [[nodiscard]] bool isFiniteAndBounded() const noexcept;
};

class DSPMapper
{
public:
    [[nodiscard]] static DSPTargets map (
        const physics::EnergyState& state,
        const MappingConfiguration& configuration = {}) noexcept;

    // For an isolated impulse and a delay with no damping, the wet tail has
    // integrated energy reflectedPower and the dry impulse has energy
    // 1-reflectedPower. Increasing feedback therefore lengthens the tail while
    // automatically reducing its first echo; it cannot manufacture energy.
    [[nodiscard]] static ReflectionPowerGains makeConservedDelayGains (
        double reflectedPower,
        double delayShare,
        double feedback,
        double maximumFeedback = 0.92) noexcept;
};
} // namespace longland::dsp
