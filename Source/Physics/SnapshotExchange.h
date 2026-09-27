#pragma once

#include "ConservationEngine.h"

#include <array>
#include <atomic>

namespace longland::physics
{
// Single-producer/single-consumer triple buffer. The audio and message threads
// never access the same slot, and neither thread waits for the other.
class SnapshotExchange
{
public:
    void publish (const PhysicsSnapshot& snapshot) noexcept
    {
        slots[producerBack] = snapshot;
        producerBack = middle.exchange (producerBack, std::memory_order_acq_rel);
    }

    [[nodiscard]] PhysicsSnapshot consume() noexcept
    {
        consumerFront = middle.exchange (consumerFront, std::memory_order_acq_rel);
        return slots[consumerFront];
    }

private:
    std::array<PhysicsSnapshot, 3> slots {};
    std::atomic<std::size_t> middle { 1 };
    std::size_t producerBack { 0 };
    std::size_t consumerFront { 2 };
};
} // namespace longland::physics

