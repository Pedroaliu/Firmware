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
    bool ddr_allocation_enabled;
};

LifecycleState g_state = {
    .domain = MemoryDomain::uninitialized,
    .ddr = DdrState::offline,
    .contained = {.base = 0U, .size = 0U},
    .ddr_allocation_enabled = false,
};

[[nodiscard]] bool contains(const PhysicalRange& range, uintptr_t address) {
    if (address < range.base) {
        return false;
    }

    return (address - range.base) < range.size;
}

} // namespace

void initialize_contained() {
    g_state.domain = MemoryDomain::contained;
    g_state.ddr = DdrState::offline;
    g_state.contained = {
        .base = reinterpret_cast<uintptr_t>(__text_start),
        .size = kQemuContainedWindowSize,
    };
    g_state.ddr_allocation_enabled = false;
}

Snapshot snapshot() {
    return {
        .domain = g_state.domain,
        .ddr = g_state.ddr,
        .contained = g_state.contained,
        .ddr_allocation_enabled = g_state.ddr_allocation_enabled,
    };
}

BackingKind backing_for(uintptr_t physical_address) {
    if (contains(g_state.contained, physical_address)) {
        switch (g_state.domain) {
        case MemoryDomain::contained:
        case MemoryDomain::transitioning:
            return BackingKind::contained;

        case MemoryDomain::mainstore:
        case MemoryDomain::early_retired:
            if (g_state.ddr == DdrState::online) {
                return BackingKind::ddr;
            }
            break;

        case MemoryDomain::uninitialized:
            break;
        }
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

    if (g_state.ddr_allocation_enabled) {
        return false;
    }

    if (backing_for(g_state.contained.base) != BackingKind::contained) {
        return false;
    }

    const uintptr_t first_address_after_contained = g_state.contained.base + g_state.contained.size;
    if (first_address_after_contained < g_state.contained.base) {
        return false;
    }

    if (backing_for(first_address_after_contained) != BackingKind::unavailable) {
        return false;
    }

    return true;
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
