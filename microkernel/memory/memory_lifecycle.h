#pragma once

#include <stddef.h>
#include <stdint.h>

namespace jixia::microkernel::memory {

/**
 * Firmware-visible memory-domain phase.
 *
 * This is deliberately independent of the concrete early-memory backend.
 * QEMU M00-07 uses a simulated contained window; Jingjie may later provide
 * boot SRAM, L2 CAR, or L3 backing-cache implementations behind the same
 * state machine.
 */
enum class MemoryDomain : uint8_t
{
    uninitialized = 0,
    contained,
    transitioning,
    mainstore,
    early_retired,
};

/**
 * Explicit DDR lifecycle. M00-07.02 begins with offline and later substeps
 * advance this state without collapsing discovery/training/layout/commit into
 * one opaque "ddr_init" bit.
 */
enum class DdrState : uint8_t
{
    offline = 0,
    discovered,
    training,
    trained,
    address_map_ready,
    decode_committed,
    online,
};

enum class BackingKind : uint8_t
{
    unavailable = 0,
    contained,
    ddr,
};

struct PhysicalRange
{
    uintptr_t base;
    size_t size;
};

struct Snapshot
{
    MemoryDomain domain;
    DdrState ddr;
    PhysicalRange contained;
    bool ddr_allocation_enabled;
};

/** Initialize the Base image's explicit pre-DDR contained-memory state. */
void initialize_contained();

[[nodiscard]] Snapshot snapshot();

/**
 * Report the currently valid backing semantics for a physical address.
 * This is a lifecycle/resource question, not a free-page allocator query.
 */
[[nodiscard]] BackingKind backing_for(uintptr_t physical_address);

/** Normal DDR pages must remain unavailable to allocators before DDR_ONLINE. */
[[nodiscard]] bool ddr_allocation_enabled();

/** M00-07.02 invariant checker used by the machine acceptance probe. */
[[nodiscard]] bool validate_contained_invariants();

[[nodiscard]] const char* domain_name(MemoryDomain domain);
[[nodiscard]] const char* ddr_state_name(DdrState state);

} // namespace jixia::microkernel::memory
