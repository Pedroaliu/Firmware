#pragma once

#include <stddef.h>
#include <stdint.h>

namespace jixia::arch::riscv::sv39 {

constexpr size_t kPageSize = 4096U;
constexpr uint64_t kSatpModeSv39 = 8ULL << 60U;

enum PteFlag : uint64_t {
    valid = 1ULL << 0U,
    read = 1ULL << 1U,
    write = 1ULL << 2U,
    execute = 1ULL << 3U,
    user = 1ULL << 4U,
    global = 1ULL << 5U,
    accessed = 1ULL << 6U,
    dirty = 1ULL << 7U,
};

using PteFlags = uint64_t;

struct AddressSpace {
    uintptr_t root_physical_address;

    [[nodiscard]] bool valid() const {
        return root_physical_address != 0U;
    }
};

/** Allocate an empty root table from PageManager. */
[[nodiscard]] AddressSpace create_address_space();

/**
 * Map one 4 KiB supervisor page, allocating intermediate tables from
 * PageManager as required. Existing leaf mappings are not overwritten.
 */
[[nodiscard]] bool map_page_4k(const AddressSpace& address_space, uintptr_t virtual_address,
                               uintptr_t physical_address, PteFlags flags);

/** Map a page-aligned range using 4 KiB leaves. */
[[nodiscard]] bool map_range_4k(const AddressSpace& address_space, uintptr_t virtual_base,
                                uintptr_t physical_base, size_t size, PteFlags flags);

[[nodiscard]] uint64_t satp_value(const AddressSpace& address_space);

/** Publish page-table changes for the current hart. */
void fence_all();
void fence_address(uintptr_t virtual_address);
void fence_instruction_stream();

} // namespace jixia::arch::riscv::sv39
