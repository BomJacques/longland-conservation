#include "EnergyTransferGraph.h"

namespace longland::physics
{
EnergyTransferGraph EnergyTransferGraph::makeDefault() noexcept
{
    EnergyTransferGraph graph;

    // These are deliberately authored pathways, not an all-to-all graph. Reverse
    // edges permit elastic restoration while retaining the same musical relation.
    graph.setBidirectional (Reservoir::direct, Reservoir::reflected);
    graph.setBidirectional (Reservoir::direct, Reservoir::spatial);
    graph.setBidirectional (Reservoir::direct, Reservoir::harmonic);
    graph.setBidirectional (Reservoir::direct, Reservoir::noise);
    graph.setBidirectional (Reservoir::direct, Reservoir::temporal);
    graph.setBidirectional (Reservoir::direct, Reservoir::low);
    graph.setBidirectional (Reservoir::spatial, Reservoir::low);

    graph.setBidirectional (Reservoir::transient, Reservoir::sustain);
    graph.setBidirectional (Reservoir::transient, Reservoir::reflected);
    graph.setBidirectional (Reservoir::transient, Reservoir::temporal);
    graph.setBidirectional (Reservoir::transient, Reservoir::mid);
    graph.setBidirectional (Reservoir::transient, Reservoir::noise);

    graph.setBidirectional (Reservoir::low, Reservoir::mid);
    graph.setBidirectional (Reservoir::low, Reservoir::high);
    graph.setBidirectional (Reservoir::low, Reservoir::sustain);
    graph.setBidirectional (Reservoir::mid, Reservoir::high);
    graph.setBidirectional (Reservoir::mid, Reservoir::harmonic);
    graph.setBidirectional (Reservoir::high, Reservoir::harmonic);
    graph.setBidirectional (Reservoir::high, Reservoir::noise);

    graph.setBidirectional (Reservoir::spatial, Reservoir::reflected);
    graph.setBidirectional (Reservoir::spatial, Reservoir::harmonic);
    graph.setBidirectional (Reservoir::sustain, Reservoir::temporal);
    graph.setBidirectional (Reservoir::sustain, Reservoir::reflected);
    return graph;
}

void EnergyTransferGraph::clear() noexcept
{
    for (auto& row : adjacency)
        row.fill (false);
}

void EnergyTransferGraph::setAllowed (Reservoir source, Reservoir destination, bool shouldAllow) noexcept
{
    if (indexOf (source) >= reservoirCount || indexOf (destination) >= reservoirCount || source == destination)
        return;
    adjacency[indexOf (source)][indexOf (destination)] = shouldAllow;
}

void EnergyTransferGraph::setBidirectional (Reservoir a, Reservoir b, bool shouldAllow) noexcept
{
    setAllowed (a, b, shouldAllow);
    setAllowed (b, a, shouldAllow);
}

bool EnergyTransferGraph::allows (Reservoir source, Reservoir destination) const noexcept
{
    if (indexOf (source) >= reservoirCount || indexOf (destination) >= reservoirCount || source == destination)
        return false;
    return adjacency[indexOf (source)][indexOf (destination)];
}
} // namespace longland::physics
