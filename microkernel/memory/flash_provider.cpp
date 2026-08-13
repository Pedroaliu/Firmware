#include "microkernel/memory/flash_provider.h"

#include "microkernel/firmware_store/ffs.h"

namespace jixia::microkernel::memory::flash_provider {
namespace {

constexpr char kBasePartition[] = "JXBASE";
constexpr char kExtendedPartition[] = "JXEXT";

void copy_from_flash(uintptr_t flash_address, void* destination, size_t size) {
    const volatile uint8_t* source = reinterpret_cast<const volatile uint8_t*>(flash_address);
    uint8_t* output = static_cast<uint8_t*>(destination);

    for (size_t index = 0U; index < size; ++index) {
        output[index] = source[index];
    }
}

} // namespace

bool image_info(ImageInfo* output) {
    if (output == nullptr) {
        return false;
    }

    firmware_store::ffs::PartitionInfo base = {};
    if (!firmware_store::ffs::find_partition(kBasePartition, &base) || base.actual_size == 0U) {
        return false;
    }

    output->base_flash_address = base.flash_address;
    output->base_size = base.actual_size;
    output->extended_flash_address = 0U;
    output->extended_size = 0U;

    firmware_store::ffs::PartitionInfo extended = {};
    if (firmware_store::ffs::find_partition(kExtendedPartition, &extended)) {
        output->extended_flash_address = extended.flash_address;
        output->extended_size = extended.actual_size;
    }

    return true;
}

bool read_extended_page(size_t page_index, uintptr_t destination_physical_address) {
    if ((destination_physical_address & (kPageSize - 1U)) != 0U) {
        return false;
    }

    firmware_store::ffs::PartitionInfo extended = {};
    if (!firmware_store::ffs::find_partition(kExtendedPartition, &extended) ||
        extended.actual_size < kPageSize) {
        return false;
    }

    const uint64_t page_offset = static_cast<uint64_t>(page_index) * kPageSize;
    if ((page_offset / kPageSize) != page_index || page_offset >= extended.actual_size ||
        extended.actual_size - page_offset < kPageSize) {
        return false;
    }

    const uintptr_t source = extended.flash_address + static_cast<uintptr_t>(page_offset);
    copy_from_flash(source, reinterpret_cast<void*>(destination_physical_address), kPageSize);
    __asm__ volatile("fence rw, rw" ::: "memory");
    return true;
}

} // namespace jixia::microkernel::memory::flash_provider
