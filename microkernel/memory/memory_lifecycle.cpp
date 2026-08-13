#include "microkernel/memory/memory_lifecycle.h"

extern "C" char __text_start[];

namespace jixia::microkernel::memory {
namespace {

/*
 * QEMU v0 semantic contained-memory capacity. This is not a claim that QEMU
 * implements an 8 MiB cache. The future hardware/simulator backend is free to
 * provide a different capacity behind the same lifecycle contract.
 */
constexpr size_t kQemuContainedWindowSize = 8U * 1024U * 1024U;

struct LifecycleState {
    MemoryDomain domain;
    DdrState ddr;
    PhysicalRange contained;
    PhysicalRange ddr_range;
    bool contained_flush_complete;
    bool ddr_allocation_enabled;
};

LifecycleState g_state = {
    .domain = MemoryDomain::uninitialized,
    .ddr = DdrState::offline,
    .contained = {.base = 0U, .size = 0U},
    .ddr_range = {.base = 0U, .size = 0U},
    .contained_flush_complete = false,
    .ddr_allocation_enabled = false,
};

[[nodiscard]] bool valid_range(const PhysicalRange& range) {
    if (range.size == 0U) {
        return false;
    }

    return range.base + range.size > range.base;
}

[[nodiscard]] bool contains(const PhysicalRange& range, uintptr_t address) {
    if (!valid_range(range) || address < range.base) {
        return false;
    }

    return (address - range.base) < range.size;
}

[[nodiscard]] bool contains_range(const PhysicalRange& outer, const PhysicalRange& inner) {
    if (!valid_range(outer) || !valid_range(inner) || inner.base < outer.base) {
        return false;
    }

    const uintptr_t outer_end = outer.base + outer.size;
    const uintptr_t inner_end = inner.base + inner.size;
    return inner_end <= outer_end;
}

} // namespace

void initialize_contained() {
    g_state.domain = MemoryDomain::contained;
    g_state.ddr = DdrState::offline;
    g_state.contained = {
        .base = reinterpret_cast<uintptr_t>(__text_start),
        .size = kQemuContainedWindowSize,
    };
    g_state.ddr_range = {.base = 0U, .size = 0U};
    g_state.contained_flush_complete = false;
    g_state.ddr_allocation_enabled = false;
}

Snapshot snapshot() {
    return {
        .domain = g_state.domain,
        .ddr = g_state.ddr,
        .contained = g_state.contained,
        .ddr_range = g_state.ddr_range,
        .contained_flush_complete = g_state.contained_flush_complete,
        .ddr_allocation_enabled = g_state.ddr_allocation_enabled,
    };
}

BackingKind backing_for(uintptr_t physical_address) {
    if ((g_state.domain == MemoryDomain::mainstore ||
         g_state.domain == MemoryDomain::early_retired) &&
        g_state.ddr == DdrState::online && contains(g_state.ddr_range, physical_address)) {
        return BackingKind::ddr;
    }

    if ((g_state.domain == MemoryDomain::contained ||
         g_state.domain == MemoryDomain::transitioning) &&
        contains(g_state.contained, physical_address)) {
        return BackingKind::contained;
    }

    return BackingKind::unavailable;
}

bool ddr_allocation_enabled() {
    return g_state.ddr_allocation_enabled;
}

bool validate_contained_invariants() {
    const uintptr_t expected_base = reinterpret_cast<uintptr_t>(__text_start);

    if (g_state.domain != MemoryDomain::contained || g_state.ddr != DdrState::offline) {
        return false;
    }

    if (g_state.contained.base != expected_base ||
        g_state.contained.size != kQemuContainedWindowSize) {
        return false;
    }

    if (valid_range(g_state.ddr_range) || g_state.contained_flush_complete ||
        g_state.ddr_allocation_enabled) {
        return false;
    }

    if (backing_for(g_state.contained.base) != BackingKind::contained) {
        return false;
    }

    const uintptr_t first_address_after_contained = g_state.contained.base + g_state.contained.size;
    if (first_address_after_contained < g_state.contained.base) {
        return false;
    }

    return backing_for(first_address_after_contained) == BackingKind::unavailable;
}

bool ddr_mark_discovered(PhysicalRange range) {
    if (g_state.domain != MemoryDomain::contained || g_state.ddr != DdrState::offline ||
        !contains_range(range, g_state.contained)) {
        return false;
    }

    g_state.ddr_range = range;
    g_state.ddr = DdrState::discovered;
    return true;
}

bool ddr_begin_training() {
    if (g_state.domain != MemoryDomain::contained || g_state.ddr != DdrState::discovered) {
        return false;
    }

    g_state.ddr = DdrState::training;
    return true;
}

bool ddr_mark_trained() {
    if (g_state.domain != MemoryDomain::contained || g_state.ddr != DdrState::training) {
        return false;
    }

    g_state.ddr = DdrState::trained;
    return true;
}

bool ddr_mark_address_map_ready() {
    if (g_state.domain != MemoryDomain::contained || g_state.ddr != DdrState::trained) {
        return false;
    }

    g_state.ddr = DdrState::address_map_ready;
    return true;
}

bool ddr_mark_decode_committed() {
    if (g_state.domain != MemoryDomain::contained || g_state.ddr != DdrState::address_map_ready) {
        return false;
    }

    g_state.ddr = DdrState::decode_committed;
    return true;
}

bool ddr_mark_online() {
    if (g_state.domain != MemoryDomain::contained || g_state.ddr != DdrState::decode_committed ||
        !contains_range(g_state.ddr_range, g_state.contained)) {
        return false;
    }

    g_state.ddr = DdrState::online;
    return true;
}

bool begin_mainstore_transition() {
    if (g_state.domain != MemoryDomain::contained || g_state.ddr != DdrState::online ||
        g_state.contained_flush_complete || g_state.ddr_allocation_enabled) {
        return false;
    }

    g_state.domain = MemoryDomain::transitioning;
    return true;
}

bool mark_contained_flush_complete() {
    if (g_state.domain != MemoryDomain::transitioning || g_state.ddr != DdrState::online ||
        g_state.contained_flush_complete) {
        return false;
    }

    g_state.contained_flush_complete = true;
    return true;
}

bool complete_mainstore_transition() {
    if (g_state.domain != MemoryDomain::transitioning || g_state.ddr != DdrState::online ||
        !g_state.contained_flush_complete || g_state.ddr_allocation_enabled) {
        return false;
    }

    g_state.domain = MemoryDomain::mainstore;
    g_state.ddr_allocation_enabled = true;
    return true;
}

bool validate_mainstore_invariants() {
    if (g_state.domain != MemoryDomain::mainstore || g_state.ddr != DdrState::online ||
        !g_state.contained_flush_complete || !g_state.ddr_allocation_enabled ||
        !contains_range(g_state.ddr_range, g_state.contained)) {
        return false;
    }

    if (backing_for(g_state.contained.base) != BackingKind::ddr) {
        return false;
    }

    const uintptr_t last_contained_address =
        g_state.contained.base + g_state.contained.size - 1U;
    if (backing_for(last_contained_address) != BackingKind::ddr) {
        return false;
    }

    const uintptr_t last_ddr_address = g_state.ddr_range.base + g_state.ddr_range.size - 1U;
    return backing_for(last_ddr_address) == BackingKind::ddr;
}

const char* domain_name(MemoryDomain domain) {
    switch (domain) {
    case MemoryDomain::uninitialized:
        return "UNINITIALIZED";
    case MemoryDomain::contained:
        return "CONTAINED";
    case MemoryDomain::transitioning:
        return "TRANSITIONING";
    case MemoryDomain::mainstore:
        return "MAINSTORE";
    case MemoryDomain::early_retired:
        return "EARLY_RETIRED";
    }

    return "UNKNOWN";
}

const char* ddr_state_name(DdrState state) {
    switch (state) {
    case DdrState::offline:
        return "OFFLINE";
    case DdrState::discovered:
        return "DISCOVERED";
    case DdrState::training:
        return "TRAINING";
    case DdrState::trained:
        return "TRAINED";
    case DdrState::address_map_ready:
        return "ADDRESS_MAP_READY";
    case DdrState::decode_committed:
        return "DECODE_COMMITTED";
    case DdrState::online:
        return "ONLINE";
    }

    return "UNKNOWN";
}

} // namespace jixia::microkernel::memory
