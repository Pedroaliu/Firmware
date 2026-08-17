#pragma once

#include <stddef.h>
#include <stdint.h>

namespace jixia::microkernel::memory::flash_provider {

constexpr size_t kPageSize = 4096U;

struct ImageInfo {
    uintptr_t base_flash_address;
    size_t base_size;
    uintptr_t extended_flash_address;
    size_t extended_size;
};

/** Read and validate the pflash-resident Jixia image header. */
[[nodiscard]] bool image_info(ImageInfo* output);

/** Copy one 4 KiB Extended-image page from memory-mapped pflash. */
[[nodiscard]] bool read_extended_page(size_t page_index, uintptr_t destination_physical_address);

} // namespace jixia::microkernel::memory::flash_provider
