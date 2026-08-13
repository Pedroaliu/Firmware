#include "microkernel/memory/flash_provider.h"

#include "boot/qemu_virt/pflash_layout.h"

namespace jixia::microkernel::memory::flash_provider {
namespace {

struct FlashHeader {
    uint64_t magic;
    uint32_t version;
    uint32_t header_size;
    uint64_t base_offset;
    uint64_t base_size;
    uint64_t base_load_address;
    uint64_t base_entry;
    uint64_t extended_offset;
    uint64_t extended_size;
};

static_assert(sizeof(FlashHeader) == JIXIA_PFLASH_HEADER_SIZE);

void copy_from_flash(uintptr_t flash_address, void* destination, size_t size) {
    const volatile uint8_t* source = reinterpret_cast<const volatile uint8_t*>(flash_address);
    uint8_t* output = static_cast<uint8_t*>(destination);

    for (size_t index = 0U; index < size; ++index) {
        output[index] = source[index];
    }
}

[[nodiscard]] bool valid_flash_range(uint64_t offset, uint64_t size) {
    if (size == 0U || offset >= JIXIA_QEMU_PFLASH_SIZE) {
        return false;
    }

    const uint64_t end = offset + size;
    return end > offset && end <= JIXIA_QEMU_PFLASH_SIZE;
}

[[nodiscard]] bool read_valid_header(FlashHeader* output) {
    if (output == nullptr) {
        return false;
    }

    copy_from_flash(JIXIA_QEMU_PFLASH_BASE + JIXIA_PFLASH_HEADER_OFFSET, output, sizeof(*output));

    if (output->magic != JIXIA_PFLASH_HEADER_MAGIC ||
        output->version != JIXIA_PFLASH_HEADER_VERSION ||
        output->header_size != JIXIA_PFLASH_HEADER_SIZE) {
        return false;
    }

    if (!valid_flash_range(output->base_offset, output->base_size)) {
        return false;
    }

    if (output->extended_size != 0U &&
        !valid_flash_range(output->extended_offset, output->extended_size)) {
        return false;
    }

    return true;
}

} // namespace

bool image_info(ImageInfo* output) {
    if (output == nullptr) {
        return false;
    }

    FlashHeader header = {};
    if (!read_valid_header(&header)) {
        return false;
    }

    output->base_flash_address = JIXIA_QEMU_PFLASH_BASE + header.base_offset;
    output->base_size = static_cast<size_t>(header.base_size);
    output->extended_flash_address =
        header.extended_size == 0U ? 0U : JIXIA_QEMU_PFLASH_BASE + header.extended_offset;
    output->extended_size = static_cast<size_t>(header.extended_size);
    return true;
}

bool read_extended_page(size_t page_index, uintptr_t destination_physical_address) {
    if ((destination_physical_address & (kPageSize - 1U)) != 0U) {
        return false;
    }

    FlashHeader header = {};
    if (!read_valid_header(&header) || header.extended_size < kPageSize) {
        return false;
    }

    const uint64_t page_offset = static_cast<uint64_t>(page_index) * kPageSize;
    if ((page_offset / kPageSize) != page_index || page_offset >= header.extended_size ||
        header.extended_size - page_offset < kPageSize) {
        return false;
    }

    const uintptr_t source =
        static_cast<uintptr_t>(JIXIA_QEMU_PFLASH_BASE + header.extended_offset + page_offset);
    copy_from_flash(source, reinterpret_cast<void*>(destination_physical_address), kPageSize);
    __asm__ volatile("fence rw, rw" ::: "memory");
    return true;
}

} // namespace jixia::microkernel::memory::flash_provider
