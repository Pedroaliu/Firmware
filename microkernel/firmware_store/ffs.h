#pragma once

#include <stddef.h>
#include <stdint.h>

namespace jixia::microkernel::firmware_store::ffs {

struct PartitionInfo {
    uint32_t flash_offset;
    uintptr_t flash_address;
    size_t allocated_size;
    size_t actual_size;
    uint32_t flags;
    uint16_t data_integrity;
    uint8_t version_check;
    uint8_t misc_flags;
};

/** Validate the configured pflash FFS table. */
[[nodiscard]] bool validate();

/** Find a top-level FFS partition by its null-terminated name. */
[[nodiscard]] bool find_partition(const char* name, PartitionInfo* output);

} // namespace jixia::microkernel::firmware_store::ffs
