#include "EnergyState.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace longland::physics
{
namespace
{
constexpr ReservoirValues equilibriumWeights {
    0.30, // direct
    0.05, // reflected
    0.10, // transient
    0.10, // sustain
    0.08, // low
    0.12, // mid
    0.08, // high
    0.04, // harmonic
    0.02, // noise
    0.05, // spatial
    0.06  // temporal
};

[[nodiscard]] double safeBudget (double value) noexcept
{
    return std::isfinite (value) ? std::clamp (value, 0.0, EnergyState::defaultBudget) : EnergyState::defaultBudget;
}
} // namespace

EnergyState EnergyState::equilibrium (double totalBudget) noexcept
{
    EnergyState result;
    result.budget = safeBudget (totalBudget);
    for (std::size_t i = 0; i < reservoirCount; ++i)
        result.amount[i] = equilibriumWeights[i] * result.budget;
    return result;
}

double EnergyState::sum() const noexcept
{
    // Kahan summation keeps the debug error meaningful after very long runs.
    double result = 0.0;
    double correction = 0.0;
    for (const auto value : amount)
    {
        const auto adjusted = value - correction;
        const auto next = result + adjusted;
        correction = (next - result) - adjusted;
        result = next;
    }
    return result;
}

bool EnergyState::isFinite() const noexcept
{
    if (! std::isfinite (budget))
        return false;
    return std::all_of (amount.begin(), amount.end(), [] (double value) { return std::isfinite (value); });
}

bool EnergyState::isNonNegative (double tolerance) const noexcept
{
    return std::all_of (amount.begin(), amount.end(), [tolerance] (double value) { return value >= -tolerance; });
}

bool EnergyState::isConserved (double tolerance) const noexcept
{
    return std::abs (conservationError()) <= tolerance;
}

bool EnergyState::isValid (double tolerance) const noexcept
{
    // Conservation error has a tolerance; negative energy does not. A tiny
    // negative is still an illegal reservoir value and must be projected away.
    return isFinite() && budget >= 0.0 && budget <= defaultBudget && isNonNegative (0.0) && isConserved (tolerance);
}

bool EnergyState::validateAndRenormalise() noexcept
{
    budget = safeBudget (budget);

    if (! isFinite())
    {
        *this = equilibrium (budget);
        return false;
    }

    for (auto& value : amount)
        value = std::max (0.0, value);

    if (budget == 0.0)
    {
        amount.fill (0.0);
        return true;
    }

    const auto currentSum = sum();
    if (! std::isfinite (currentSum) || currentSum <= 0.0)
    {
        *this = equilibrium (budget);
        return false;
    }

    const auto scale = budget / currentSum;
    for (auto& value : amount)
        value *= scale;

    // Put only the final floating-point residual into the largest, most stable
    // reservoir. This correction is normally many orders below audibility.
    const auto largest = static_cast<std::size_t> (std::distance (
        amount.begin(), std::max_element (amount.begin(), amount.end())));
    amount[largest] = std::max (0.0, amount[largest] + (budget - sum()));
    return true;
}

void EnergyState::setBudget (double newBudget) noexcept
{
    const auto target = safeBudget (newBudget);
    if (target == budget)
        return;

    if (budget > 0.0 && isFinite())
    {
        const auto scale = target / budget;
        for (auto& value : amount)
            value *= scale;
        budget = target;
        validateAndRenormalise();
        return;
    }

    *this = equilibrium (target);
}
} // namespace longland::physics
