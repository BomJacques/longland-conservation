#pragma once

#include "EnergyState.h"
#include "EnergyTransferGraph.h"

#include <array>
#include <cstddef>

namespace longland::physics
{
enum class TransferCurve
{
    linear,
    gentle,
    responsive
};

struct EnergyRequest
{
    Reservoir destination { Reservoir::direct };
    double requestedAmount {};
    std::array<Reservoir, reservoirCount> preferredDonors {};
    std::size_t preferredDonorCount {};
    TransferCurve curve { TransferCurve::linear };
    bool allowGraphFallback { true };

    void addPreferredDonor (Reservoir donor) noexcept;
};

struct EnergyTransfer
{
    Reservoir source { Reservoir::direct };
    Reservoir destination { Reservoir::direct };
    double amount {};
};

enum class TransferStatus
{
    applied,
    partiallyApplied,
    exhausted,
    invalidRequest,
    invalidState
};

struct TransferResult
{
    std::array<EnergyTransfer, reservoirCount> transfers {};
    std::size_t transferCount {};
    double requestedAmount {};
    double shapedAmount {};
    double transferredAmount {};
    TransferStatus status { TransferStatus::invalidRequest };

    [[nodiscard]] bool fulfilled() const noexcept { return status == TransferStatus::applied; }
};

class ConservationSolver
{
public:
    [[nodiscard]] TransferResult apply (EnergyState& state,
                                        const EnergyTransferGraph& graph,
                                        const EnergyRequest& request) const noexcept;

private:
    [[nodiscard]] static double shapeAmount (double requested, double availableRoom,
                                             TransferCurve curve) noexcept;
};
} // namespace longland::physics
