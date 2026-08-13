#pragma once

#include <stddef.h>
#include <stdint.h>

#include "microkernel/memory/memory_lifecycle.h"

namespace jixia::microkernel::memory::page_manager {

constexpr size_t kPageSize = 4096U;

struct Allocation {
    uintptr_t physical_address;
    BackingKind backing;

    [[nodiscard]] bool valid() const {
        return physical_address != 0U && backing != BackingKind::unavailable;
    }
};

/** Reset all allocator ranges. Intended for deterministic early boot only. */
void reset();

/**
 * Add one page-aligned allocator-owned physical range.
 *
 * System resource classification remains separate from this allocator. The
 * caller may add only ranges already known to be legal firmware RAM.
 */
[[nodiscard]] bool add_range(uintptr_t base, size_t size, BackingKind backing);

/** Register the linker-reserved contained bootstrap pool. */
[[nodiscard]] bool add_contained_bootstrap_pool();

/**
 * Relabel existing managed ranges after a same-address backing transition.
 * base/next/end are deliberately untouched; live allocations do not move.
 */
[[nodiscard]] size_t promote_backing(BackingKind from, BackingKind to);

/** Allocate and zero one physical page. No free/coalescing policy in M00-07. */
[[nodiscard]] Allocation allocate_page();

[[nodiscard]] size_t remaining_pages(BackingKind backing);

} // namespace jixia::microkernel::memory::page_manager
