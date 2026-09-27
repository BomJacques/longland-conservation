#include "ConservationSolver.h"

#include <algorithm>
#include <cmath>

namespace longland::physics
{
namespace
{
constexpr double transferEpsilon = 1.0e-14;

[[nodiscard]] bool validReservoir (Reservoir reservoir) noexcept
{
    return indexOf (reservoir) < reservoirCount;
}
} // namespace

void EnergyRequest::addPreferredDonor (Reservoir donor) noexcept
{
    if (preferredDonorCount >= preferredDonors.size() || ! validReservoir (donor) || donor == destination)
        return;
    preferredDonors[preferredDonorCount++] = donor;
}

double ConservationSolver::shapeAmount (double requested, double availableRoom, TransferCurve curve) noexcept
{
    if (availableRoom <= 0.0)
        return 0.0;

    const auto clamped = std::clamp (requested, 0.0, availableRoom);
    const auto x = clamped / availableRoom;
    switch (curve)
    {
        case TransferCurve::gentle:     return availableRoom * x * x;
        case TransferCurve::responsive: return availableRoom * (1.0 - std::exp (-2.0 * x)) / (1.0 - std::exp (-2.0));
        case TransferCurve::linear:     break;
    }
    return clamped;
}

TransferResult ConservationSolver::apply (EnergyState& state, const EnergyTransferGraph& graph,
                                           const EnergyRequest& request) const noexcept
{
    TransferResult result;
    result.requestedAmount = request.requestedAmount;

    if (! state.isValid())
    {
        state.validateAndRenormalise();
        result.status = TransferStatus::invalidState;
        return result;
    }

    if (! validReservoir (request.destination) || ! std::isfinite (request.requestedAmount)
        || request.requestedAmount < 0.0)
    {
        result.status = TransferStatus::invalidRequest;
        return result;
    }

    const auto room = std::max (0.0, state.budget - state[request.destination]);
    result.shapedAmount = shapeAmount (request.requestedAmount, room, request.curve);
    auto remaining = result.shapedAmount;

    std::array<bool, reservoirCount> alreadyTried {};
    const auto takeFrom = [&] (Reservoir donor, double maximum)
    {
        if (remaining <= transferEpsilon || maximum <= 0.0 || ! validReservoir (donor)
            || alreadyTried[indexOf (donor)] || ! graph.allows (donor, request.destination))
            return;

        alreadyTried[indexOf (donor)] = true;
        const auto moved = std::min ({ remaining, maximum, state[donor] });
        if (moved <= transferEpsilon)
            return;

        state[donor] -= moved;
        state[request.destination] += moved;
        result.transfers[result.transferCount++] = { donor, request.destination, moved };
        result.transferredAmount += moved;
        remaining -= moved;
    };

    for (std::size_t i = 0; i < std::min (request.preferredDonorCount, reservoirCount); ++i)
        takeFrom (request.preferredDonors[i], remaining);

    if (request.allowGraphFallback && remaining > transferEpsilon)
    {
        double fallbackAvailable = 0.0;
        for (std::size_t i = 0; i < reservoirCount; ++i)
        {
            const auto donor = static_cast<Reservoir> (i);
            if (! alreadyTried[i] && graph.allows (donor, request.destination))
                fallbackAvailable += state[donor];
        }

        const auto target = std::min (remaining, fallbackAvailable);
        const auto initialTarget = target;
        for (std::size_t i = 0; i < reservoirCount && remaining > transferEpsilon; ++i)
        {
            const auto donor = static_cast<Reservoir> (i);
            if (alreadyTried[i] || ! graph.allows (donor, request.destination) || fallbackAvailable <= 0.0)
                continue;
            takeFrom (donor, initialTarget * state[donor] / fallbackAvailable);
        }

        // Round-off or a donor changing during the proportional pass can leave a
        // minute remainder. A second deterministic pass consumes only that tail.
        for (std::size_t i = 0; i < reservoirCount && remaining > transferEpsilon; ++i)
        {
            const auto donor = static_cast<Reservoir> (i);
            if (! alreadyTried[i] && graph.allows (donor, request.destination))
                takeFrom (donor, remaining);
        }
    }

    if (! state.isValid())
        state.validateAndRenormalise();

    if (result.shapedAmount <= transferEpsilon)
        result.status = TransferStatus::exhausted;
    else if (remaining <= transferEpsilon)
        result.status = TransferStatus::applied;
    else if (result.transferredAmount > transferEpsilon)
        result.status = TransferStatus::partiallyApplied;
    else
        result.status = TransferStatus::exhausted;
    return result;
}
} // namespace longland::physics
