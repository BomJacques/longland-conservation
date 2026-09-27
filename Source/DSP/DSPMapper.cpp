#include "DSPMapper.h"

#include <algorithm>
#include <cmath>

namespace longland::dsp
{
namespace
{
constexpr double epsilon = 1.0e-12;
constexpr double neutralTransientBalance = 0.5;
constexpr double neutralHarmonicRatio = 0.04 / (0.04 + 0.30 + 0.10);
constexpr double neutralNoiseRatio = 0.02 / (0.02 + 0.04 + 0.30);
constexpr double neutralSpatialRatio = 0.05 / (0.05 + 0.30);
constexpr std::array<double, 3> neutralSpectrum {
    0.08 / 0.28, 0.12 / 0.28, 0.08 / 0.28
};

[[nodiscard]] double sanitiseUnit (double value, double fallback = 0.0) noexcept
{
    return std::isfinite (value) ? std::clamp (value, 0.0, 1.0) : fallback;
}

[[nodiscard]] double smoothstep (double value) noexcept
{
    const auto x = sanitiseUnit (value);
    return x * x * (3.0 - 2.0 * x);
}

[[nodiscard]] double ratioOr (double numerator, double denominator, double fallback) noexcept
{
    if (! std::isfinite (numerator) || ! std::isfinite (denominator) || denominator <= epsilon)
        return fallback;
    return sanitiseUnit (numerator / denominator, fallback);
}

[[nodiscard]] double positiveDisplacement (double value, double neutral) noexcept
{
    return sanitiseUnit ((value - neutral) / std::max (epsilon, 1.0 - neutral));
}

[[nodiscard]] double negativeDisplacement (double value, double neutral) noexcept
{
    return sanitiseUnit ((neutral - value) / std::max (epsilon, neutral));
}

[[nodiscard]] double logarithmicMap (double low, double high, double position) noexcept
{
    low = std::max (epsilon, low);
    high = std::max (low, high);
    return std::exp (std::log (low) + sanitiseUnit (position) * std::log (high / low));
}

[[nodiscard]] physics::EnergyState safeState (const physics::EnergyState& source) noexcept
{
    if (source.isValid())
        return source;

    auto repaired = source;
    repaired.validateAndRenormalise();
    return repaired;
}
} // namespace

bool DSPTargets::isFiniteAndBounded() const noexcept
{
    const std::array scalarValues {
        outputGain, directPower, delayPower, reverbPower,
        reflection.direct, reflection.delayInput, reflection.feedback,
        reverbInputGain, delaySeconds, reverbT60Seconds, reflectionLowpassHz,
        transientSegmentGain, sustainSegmentGain, harmonicBlend, harmonicDrive,
        noiseTextureBlend, midGain, sideGain
    };

    if (! std::all_of (scalarValues.begin(), scalarValues.end(), [] (double value) {
            return std::isfinite (value) && value >= 0.0;
        }))
        return false;

    if (! std::all_of (spectralBandGains.begin(), spectralBandGains.end(), [] (double value) {
            return std::isfinite (value) && value >= 0.0 && value <= 1.0;
        }))
        return false;

    return outputGain <= 1.0
        && directPower <= 1.0 && delayPower <= 1.0 && reverbPower <= 1.0
        && reflection.direct <= 1.0 && reflection.delayInput <= 1.0
        && reflection.feedback < 1.0 && reverbInputGain <= 1.0
        && transientSegmentGain <= 1.0 && sustainSegmentGain <= 1.0
        && harmonicBlend <= 1.0 && harmonicDrive <= 16.0
        && noiseTextureBlend <= 0.5 && midGain <= 1.0 && sideGain <= 1.0;
}

ReflectionPowerGains DSPMapper::makeConservedDelayGains (
    double reflectedPower,
    double delayShare,
    double feedback,
    double maximumFeedback) noexcept
{
    const auto q = sanitiseUnit (reflectedPower);
    const auto delayFraction = sanitiseUnit (delayShare, 1.0);
    const auto feedbackLimit = std::clamp (
        std::isfinite (maximumFeedback) ? maximumFeedback : 0.92, 0.0, 0.998);
    const auto g = std::clamp (std::isfinite (feedback) ? feedback : 0.0,
                              -feedbackLimit, feedbackLimit);
    const auto delayPower = q * delayFraction;

    ReflectionPowerGains result;
    result.direct = std::sqrt (std::max (0.0, 1.0 - q));
    result.delayInput = std::sqrt (std::max (0.0, delayPower * (1.0 - g * g)));
    result.feedback = g;
    return result;
}

DSPTargets DSPMapper::map (
    const physics::EnergyState& source,
    const MappingConfiguration& sourceConfiguration) noexcept
{
    const auto state = safeState (source);
    DSPTargets targets;

    MappingConfiguration configuration = sourceConfiguration;
    configuration.friction = sanitiseUnit (configuration.friction, 0.5);
    configuration.maximumFeedback = std::clamp (
        std::isfinite (configuration.maximumFeedback) ? configuration.maximumFeedback : 0.92,
        0.0, 0.998);
    configuration.minimumDelaySeconds = std::clamp (
        std::isfinite (configuration.minimumDelaySeconds) ? configuration.minimumDelaySeconds : 0.030,
        0.001, 4.0);
    configuration.maximumDelaySeconds = std::clamp (
        std::isfinite (configuration.maximumDelaySeconds) ? configuration.maximumDelaySeconds : 0.750,
        configuration.minimumDelaySeconds, 4.0);
    configuration.minimumReverbSeconds = std::clamp (
        std::isfinite (configuration.minimumReverbSeconds) ? configuration.minimumReverbSeconds : 0.20,
        0.05, 30.0);
    configuration.maximumReverbSeconds = std::clamp (
        std::isfinite (configuration.maximumReverbSeconds) ? configuration.maximumReverbSeconds : 8.0,
        configuration.minimumReverbSeconds, 30.0);

    if (state.budget <= epsilon)
    {
        targets.outputGain = 0.0;
        targets.directPower = 0.0;
        targets.reflection.direct = 0.0;
        return targets;
    }

    targets.outputGain = std::sqrt (sanitiseUnit (state.budget / physics::EnergyState::defaultBudget));

    const auto direct = state[physics::Reservoir::direct];
    const auto reflected = state[physics::Reservoir::reflected];
    const auto transient = state[physics::Reservoir::transient];
    const auto sustain = state[physics::Reservoir::sustain];
    const auto low = state[physics::Reservoir::low];
    const auto mid = state[physics::Reservoir::mid];
    const auto high = state[physics::Reservoir::high];
    const auto harmonic = state[physics::Reservoir::harmonic];
    const auto noise = state[physics::Reservoir::noise];
    const auto spatial = state[physics::Reservoir::spatial];
    const auto temporal = state[physics::Reservoir::temporal];

    // The conditional processors below describe balance inside subsystems, but
    // a depleted audible subsystem must not be renormalised back to full power.
    // Neutral Direct+Reflected energy (0.35 of budget) is unity; migration away
    // from both domains creates a real attenuation consequence and never boosts.
    constexpr double neutralAudiblePairShare = 0.35;
    const auto audiblePairShare = ratioOr (direct + reflected, state.budget, 0.0);
    targets.outputGain *= std::sqrt (std::clamp (
        audiblePairShare / neutralAudiblePairShare, 0.0, 1.0));

    const auto reflectedRatio = ratioOr (reflected, direct + reflected, 0.0);
    const auto transientRatio = ratioOr (transient, transient + sustain, neutralTransientBalance);
    const auto temporalRatio = ratioOr (temporal, temporal + transient + sustain, 0.0);
    const auto spatialRatio = ratioOr (spatial, spatial + direct, neutralSpatialRatio);

    const auto reverbShare = std::clamp (
        0.20 + 0.55 * spatialRatio + 0.25 * (1.0 - transientRatio), 0.15, 0.85);
    targets.directPower = 1.0 - reflectedRatio;
    targets.delayPower = reflectedRatio * (1.0 - reverbShare);
    targets.reverbPower = reflectedRatio * reverbShare;

    const auto persistence = smoothstep (temporalRatio) * (1.0 - 0.80 * configuration.friction);
    const auto feedback = configuration.maximumFeedback * (0.05 + 0.95 * persistence);
    targets.reflection = makeConservedDelayGains (
        reflectedRatio, 1.0 - reverbShare, feedback, configuration.maximumFeedback);
    targets.reverbInputGain = std::sqrt (targets.reverbPower);
    targets.delaySeconds = logarithmicMap (
        configuration.minimumDelaySeconds, configuration.maximumDelaySeconds, temporalRatio);

    const auto reverbPersistence = sanitiseUnit (
        0.65 * temporalRatio + 0.35 * (1.0 - transientRatio));
    const auto dampedReverbPersistence = reverbPersistence * (1.0 - 0.85 * configuration.friction);
    targets.reverbT60Seconds = logarithmicMap (
        configuration.minimumReverbSeconds,
        configuration.maximumReverbSeconds,
        dampedReverbPersistence);

    const auto spectralTotal = low + mid + high;
    const std::array spectrum {
        ratioOr (low, spectralTotal, neutralSpectrum[0]),
        ratioOr (mid, spectralTotal, neutralSpectrum[1]),
        ratioOr (high, spectralTotal, neutralSpectrum[2])
    };

    const auto brightness = sanitiseUnit (
        0.65 + 1.50 * (spectrum[2] - neutralSpectrum[2])
             - 0.80 * (spectrum[0] - neutralSpectrum[0]), 0.65);
    targets.reflectionLowpassHz = logarithmicMap (1200.0, 18000.0, brightness);

    if (transientRatio >= neutralTransientBalance)
    {
        const auto displacement = positiveDisplacement (transientRatio, neutralTransientBalance);
        targets.sustainSegmentGain = 1.0 - 0.75 * displacement;
    }
    else
    {
        const auto displacement = negativeDisplacement (transientRatio, neutralTransientBalance);
        targets.transientSegmentGain = 1.0 - 0.75 * displacement;
    }

    std::array<double, 3> rawBandGains {};
    double maximumRawGain = 0.0;
    double spectralDisplacement = 0.0;
    for (std::size_t i = 0; i < rawBandGains.size(); ++i)
    {
        rawBandGains[i] = std::sqrt ((spectrum[i] + epsilon) / (neutralSpectrum[i] + epsilon));
        maximumRawGain = std::max (maximumRawGain, rawBandGains[i]);
        spectralDisplacement += std::abs (spectrum[i] - neutralSpectrum[i]);
    }

    const auto spectralDepth = sanitiseUnit (spectralDisplacement / 0.70);
    for (std::size_t i = 0; i < targets.spectralBandGains.size(); ++i)
    {
        const auto attenuationOnly = std::clamp (rawBandGains[i] / std::max (epsilon, maximumRawGain),
                                                 0.0630957344, 1.0); // -24 dB floor
        targets.spectralBandGains[i] = 1.0 + spectralDepth * (attenuationOnly - 1.0);
    }

    const auto harmonicRatio = ratioOr (harmonic, harmonic + direct + transient, neutralHarmonicRatio);
    targets.harmonicBlend = positiveDisplacement (harmonicRatio, neutralHarmonicRatio);
    targets.harmonicDrive = 1.0 + 15.0 * targets.harmonicBlend * targets.harmonicBlend;

    const auto noiseRatio = ratioOr (noise, noise + harmonic + direct, neutralNoiseRatio);
    targets.noiseTextureBlend = 0.5 * positiveDisplacement (noiseRatio, neutralNoiseRatio);

    if (spatialRatio >= neutralSpatialRatio)
    {
        const auto width = positiveDisplacement (spatialRatio, neutralSpatialRatio);
        targets.midGain = std::pow (10.0, (-9.0 * width) / 20.0);
    }
    else
    {
        const auto narrowing = negativeDisplacement (spatialRatio, neutralSpatialRatio);
        targets.sideGain = std::pow (10.0, (-18.0 * narrowing) / 20.0);
    }

    if (! targets.isFiniteAndBounded())
        return map (physics::EnergyState::equilibrium (state.budget), MappingConfiguration {});
    return targets;
}
} // namespace longland::dsp
