#pragma once

#include "EnergyState.h"

#include <array>

namespace longland::physics
{
class EnergyTransferGraph
{
public:
    [[nodiscard]] static EnergyTransferGraph makeDefault() noexcept;

    void clear() noexcept;
    void setAllowed (Reservoir source, Reservoir destination, bool shouldAllow = true) noexcept;
    void setBidirectional (Reservoir a, Reservoir b, bool shouldAllow = true) noexcept;
    [[nodiscard]] bool allows (Reservoir source, Reservoir destination) const noexcept;

private:
    std::array<std::array<bool, reservoirCount>, reservoirCount> adjacency {};
};
} // namespace longland::physics
