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
enum class MemoryDomain : uint8_t {
    uninitialized = 0,
    contained,
    transitioning,
    mainstore,
    early_retired,
};

/**
 * Explicit DDR lifecycle. Later hardware-specific implementations replace the
 * stub work behind these transitions without collapsing them into one bit.
 */
enum class DdrState : uint8_t {
    offline = 0,
    discovered,
    training,
    trained,
    address_map_ready,
    decode_committed,
    online,
};

enum class BackingKind : uint8_t {
    unavailable = 0,
    contained,
    ddr,
};

struct PhysicalRange {
    uintptr_t base;
    size_t size;
};

struct Snapshot {
    MemoryDomain domain;
    DdrState ddr;
    PhysicalRange contained;
    PhysicalRange ddr_range;
    bool contained_flush_complete;
    bool ddr_allocation_enabled;
};

/** Initialize the Base image's explicit pre-DDR contained-memory state. */
void initialize_contained();

[[nodiscard]] Snapshot snapshot();

/**
 * Report the firmware-visible backing semantics for a physical address.
 * This is a lifecycle/resource question, not a free-page allocator query.
 */
[[nodiscard]] BackingKind backing_for(uintptr_t physical_address);

/** Normal DDR pages remain allocator-invisible until mainstore transition ends. */
[[nodiscard]] bool ddr_allocation_enabled();

/** M00-07.02 invariant checker used by the machine acceptance probe. */
[[nodiscard]] bool validate_contained_invariants();

/** Strict, one-way DDR bring-up transitions used by M00-07.04. */
[[nodiscard]] bool ddr_mark_discovered(PhysicalRange range);
[[nodiscard]] bool ddr_begin_training();
[[nodiscard]] bool ddr_mark_trained();
[[nodiscard]] bool ddr_mark_address_map_ready();
[[nodiscard]] bool ddr_mark_decode_committed();
[[nodiscard]] bool ddr_mark_online();

/**
 * Mainstore transition is separate from DDR hardware becoming operational.
 * This mirrors Hostboot's ordering: make memory a valid castout target first,
 * then stop/flush/leave contained mode, then expose normal mainstore pages.
 */
[[nodiscard]] bool begin_mainstore_transition();
[[nodiscard]] bool mark_contained_flush_complete();
[[nodiscard]] bool complete_mainstore_transition();
[[nodiscard]] bool enable_mainstore_allocation();

[[nodiscard]] bool validate_mainstore_invariants();

[[nodiscard]] const char* domain_name(MemoryDomain domain);
[[nodiscard]] const char* ddr_state_name(DdrState state);

} // namespace jixia::microkernel::memory
