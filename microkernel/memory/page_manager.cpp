#include "microkernel/memory/page_manager.h"

extern "C" char __early_page_pool_start[];
extern "C" char __early_page_pool_end[];

namespace jixia::microkernel::memory::page_manager {
namespace {

constexpr size_t kMaxRanges = 4U;

struct ManagedRange {
    uintptr_t base;
    uintptr_t next;
    uintptr_t end;
    BackingKind backing;
};

ManagedRange g_ranges[kMaxRanges] = {};
size_t g_range_count = 0U;

[[nodiscard]] bool is_page_aligned(uintptr_t value) {
    return (value & (kPageSize - 1U)) == 0U;
}

[[nodiscard]] bool overlaps(uintptr_t base, uintptr_t end, const ManagedRange& range) {
    return base < range.end && range.base < end;
}

void zero_page(uintptr_t physical_address) {
    volatile uint64_t* words = reinterpret_cast<volatile uint64_t*>(physical_address);
    constexpr size_t kWordsPerPage = kPageSize / sizeof(uint64_t);

    for (size_t index = 0U; index < kWordsPerPage; ++index) {
        words[index] = 0U;
    }
}

[[nodiscard]] Allocation allocate_from(BackingKind backing) {
    for (size_t index = 0U; index < g_range_count; ++index) {
        ManagedRange& range = g_ranges[index];
        if (range.backing != backing || range.next >= range.end) {
            continue;
        }

        const uintptr_t physical_address = range.next;
        range.next += kPageSize;
        zero_page(physical_address);
        return {.physical_address = physical_address, .backing = backing};
    }

    return {.physical_address = 0U, .backing = BackingKind::unavailable};
}

} // namespace

void reset() {
    for (size_t index = 0U; index < kMaxRanges; ++index) {
        g_ranges[index] = {};
    }
    g_range_count = 0U;
}

bool add_range(uintptr_t base, size_t size, BackingKind backing) {
    if (g_range_count >= kMaxRanges || backing == BackingKind::unavailable || size == 0U ||
        !is_page_aligned(base) || (size % kPageSize) != 0U) {
        return false;
    }

    const uintptr_t end = base + size;
    if (end <= base) {
        return false;
    }

    for (size_t index = 0U; index < g_range_count; ++index) {
        if (overlaps(base, end, g_ranges[index])) {
            return false;
        }
    }

    if (backing == BackingKind::contained) {
        if (memory::backing_for(base) != BackingKind::contained ||
            memory::backing_for(end - 1U) != BackingKind::contained) {
            return false;
        }
    }

    if (backing == BackingKind::ddr) {
        if (memory::backing_for(base) != BackingKind::ddr ||
            memory::backing_for(end - 1U) != BackingKind::ddr) {
            return false;
        }
    }

    g_ranges[g_range_count] = {
        .base = base,
        .next = base,
        .end = end,
        .backing = backing,
    };
    ++g_range_count;
    return true;
}

bool add_contained_bootstrap_pool() {
    const uintptr_t base = reinterpret_cast<uintptr_t>(__early_page_pool_start);
    const uintptr_t end = reinterpret_cast<uintptr_t>(__early_page_pool_end);
    if (end <= base) {
        return false;
    }

    return add_range(base, end - base, BackingKind::contained);
}

size_t promote_backing(BackingKind from, BackingKind to) {
    if (from == to || from == BackingKind::unavailable || to == BackingKind::unavailable) {
        return 0U;
    }

    size_t promotable_ranges = 0U;

    /* Validate the complete transition before mutating any allocator range. */
    for (size_t index = 0U; index < g_range_count; ++index) {
        const ManagedRange& range = g_ranges[index];
        if (range.backing != from) {
            continue;
        }

        if (memory::backing_for(range.base) != to || memory::backing_for(range.end - 1U) != to) {
            return 0U;
        }

        ++promotable_ranges;
    }

    if (promotable_ranges == 0U) {
        return 0U;
    }

    for (size_t index = 0U; index < g_range_count; ++index) {
        ManagedRange& range = g_ranges[index];
        if (range.backing == from) {
            range.backing = to;
        }
    }

    return promotable_ranges;
}

Allocation allocate_page() {
    const Snapshot state = memory::snapshot();

    if (state.ddr_allocation_enabled) {
        return allocate_from(BackingKind::ddr);
    }

    if (state.domain == MemoryDomain::contained) {
        return allocate_from(BackingKind::contained);
    }

    return {.physical_address = 0U, .backing = BackingKind::unavailable};
}

size_t remaining_pages(BackingKind backing) {
    size_t count = 0U;

    for (size_t index = 0U; index < g_range_count; ++index) {
        const ManagedRange& range = g_ranges[index];
        if (range.backing == backing && range.next < range.end) {
            count += static_cast<size_t>((range.end - range.next) / kPageSize);
        }
    }

    return count;
}

} // namespace jixia::microkernel::memory::page_manager
