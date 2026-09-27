#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace longland::physics
{
enum class Reservoir : std::size_t
{
    direct,
    reflected,
    transient,
    sustain,
    low,
    mid,
    high,
    harmonic,
    noise,
    spatial,
    temporal,
    count
};

inline constexpr std::size_t reservoirCount = static_cast<std::size_t> (Reservoir::count);
using ReservoirValues = std::array<double, reservoirCount>;

[[nodiscard]] constexpr std::size_t indexOf (Reservoir reservoir) noexcept
{
    return static_cast<std::size_t> (reservoir);
}

[[nodiscard]] constexpr std::string_view reservoirName (Reservoir reservoir) noexcept
{
    constexpr std::array names {
        std::string_view { "Direct" }, std::string_view { "Reflected" },
        std::string_view { "Transient" }, std::string_view { "Sustain" },
        std::string_view { "Low" }, std::string_view { "Mid" },
        std::string_view { "High" }, std::string_view { "Harmonic" },
        std::string_view { "Noise" }, std::string_view { "Spatial" },
        std::string_view { "Temporal" }
    };
    return indexOf (reservoir) < names.size() ? names[indexOf (reservoir)] : std::string_view { "Invalid" };
}

struct EnergyState
{
    static constexpr double defaultBudget = 1.0;
    static constexpr double conservationTolerance = 1.0e-10;

    ReservoirValues amount {};
    double budget { defaultBudget };

    [[nodiscard]] static EnergyState equilibrium (double totalBudget = defaultBudget) noexcept;

    [[nodiscard]] double& operator[] (Reservoir reservoir) noexcept { return amount[indexOf (reservoir)]; }
    [[nodiscard]] double operator[] (Reservoir reservoir) const noexcept { return amount[indexOf (reservoir)]; }
    [[nodiscard]] double sum() const noexcept;
    [[nodiscard]] double conservationError() const noexcept { return budget - sum(); }
    [[nodiscard]] bool isFinite() const noexcept;
    [[nodiscard]] bool isNonNegative (double tolerance = conservationTolerance) const noexcept;
    [[nodiscard]] bool isConserved (double tolerance = conservationTolerance) const noexcept;
    [[nodiscard]] bool isValid (double tolerance = conservationTolerance) const noexcept;

    // Repairs invalid values and projects the state onto the non-negative simplex.
    // Returns false when a corrupt state had to be replaced by equilibrium.
    bool validateAndRenormalise() noexcept;
    void setBudget (double newBudget) noexcept;
};
} // namespace longland::physics
