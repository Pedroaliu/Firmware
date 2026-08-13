#include "microkernel/firmware_store/ffs.h"

#include "boot/common/ffs_format.h"
#include "boot/qemu_virt/pflash_layout.h"

namespace jixia::microkernel::firmware_store::ffs {
namespace {

struct ParsedHeader {
    uint32_t table_size;
    uint32_t entry_count;
    uint32_t block_size;
    uint32_t block_count;
};

[[nodiscard]] const volatile uint8_t* flash_at(uint32_t offset) {
    return reinterpret_cast<const volatile uint8_t*>(JIXIA_QEMU_PFLASH_BASE + offset);
}

[[nodiscard]] uint32_t load_be32(const volatile uint8_t* address) {
    return (static_cast<uint32_t>(address[0]) << 24U) |
           (static_cast<uint32_t>(address[1]) << 16U) |
           (static_cast<uint32_t>(address[2]) << 8U) | static_cast<uint32_t>(address[3]);
}

[[nodiscard]] uint16_t load_be16(const volatile uint8_t* address) {
    return static_cast<uint16_t>((static_cast<uint16_t>(address[0]) << 8U) |
                                 static_cast<uint16_t>(address[1]));
}

[[nodiscard]] bool checksum_is_zero(const volatile uint8_t* address, size_t size) {
    if ((size % sizeof(uint32_t)) != 0U) {
        return false;
    }

    uint32_t checksum = 0U;
    for (size_t offset = 0U; offset < size; offset += sizeof(uint32_t)) {
        checksum ^= load_be32(address + offset);
    }

    return checksum == 0U;
}

[[nodiscard]] bool name_matches(const volatile uint8_t* entry, const char* expected) {
    if (expected == nullptr) {
        return false;
    }

    for (size_t index = 0U; index <= JIXIA_FFS_PART_NAME_MAX; ++index) {
        const char actual = static_cast<char>(entry[JIXIA_FFS_ENTRY_NAME_OFFSET + index]);
        const char wanted = expected[index];

        if (actual != wanted) {
            return false;
        }

        if (wanted == '\0') {
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool parse_header(ParsedHeader* output) {
    if (output == nullptr) {
        return false;
    }

    const volatile uint8_t* const header = flash_at(JIXIA_PFLASH_TOC_OFFSET);
    if (!checksum_is_zero(header, JIXIA_FFS_HEADER_SIZE)) {
        return false;
    }

    if (load_be32(header + JIXIA_FFS_HDR_MAGIC_OFFSET) != JIXIA_FFS_MAGIC ||
        load_be32(header + JIXIA_FFS_HDR_VERSION_OFFSET) != JIXIA_FFS_VERSION ||
        load_be32(header + JIXIA_FFS_HDR_ENTRY_SIZE_OFFSET) != JIXIA_FFS_ENTRY_SIZE) {
        return false;
    }

    const uint32_t table_blocks = load_be32(header + JIXIA_FFS_HDR_SIZE_OFFSET);
    const uint32_t entry_count = load_be32(header + JIXIA_FFS_HDR_ENTRY_COUNT_OFFSET);
    const uint32_t block_size = load_be32(header + JIXIA_FFS_HDR_BLOCK_SIZE_OFFSET);
    const uint32_t block_count = load_be32(header + JIXIA_FFS_HDR_BLOCK_COUNT_OFFSET);

    if (entry_count == 0U || entry_count > JIXIA_PFLASH_MAX_FFS_ENTRIES ||
        block_size != JIXIA_PFLASH_BLOCK_SIZE || block_count != JIXIA_PFLASH_BLOCK_COUNT) {
        return false;
    }

    const uint64_t table_size = static_cast<uint64_t>(table_blocks) * block_size;
    const uint64_t device_size = static_cast<uint64_t>(block_count) * block_size;
    const uint64_t entries_end =
        JIXIA_FFS_HEADER_SIZE + static_cast<uint64_t>(entry_count) * JIXIA_FFS_ENTRY_SIZE;

    if (table_size != JIXIA_PFLASH_TOC_SIZE || device_size != JIXIA_QEMU_PFLASH_SIZE ||
        entries_end > table_size) {
        return false;
    }

    output->table_size = static_cast<uint32_t>(table_size);
    output->entry_count = entry_count;
    output->block_size = block_size;
    output->block_count = block_count;
    return true;
}

[[nodiscard]] bool parse_partition(const volatile uint8_t* entry, const ParsedHeader& header,
                                   PartitionInfo* output) {
    if (!checksum_is_zero(entry, JIXIA_FFS_ENTRY_SIZE) || output == nullptr) {
        return false;
    }

    const uint32_t base_blocks = load_be32(entry + JIXIA_FFS_ENTRY_BASE_OFFSET);
    const uint32_t size_blocks = load_be32(entry + JIXIA_FFS_ENTRY_SIZE_OFFSET);
    const uint32_t actual_size = load_be32(entry + JIXIA_FFS_ENTRY_ACTUAL_OFFSET);

    const uint64_t flash_offset = static_cast<uint64_t>(base_blocks) * header.block_size;
    const uint64_t allocated_size = static_cast<uint64_t>(size_blocks) * header.block_size;
    const uint64_t flash_end = flash_offset + allocated_size;
    const uint64_t device_size = static_cast<uint64_t>(header.block_count) * header.block_size;

    if (size_blocks == 0U || flash_end <= flash_offset || flash_end > device_size ||
        actual_size > allocated_size) {
        return false;
    }

    output->flash_offset = static_cast<uint32_t>(flash_offset);
    output->flash_address = JIXIA_QEMU_PFLASH_BASE + static_cast<uintptr_t>(flash_offset);
    output->allocated_size = static_cast<size_t>(allocated_size);
    output->actual_size = static_cast<size_t>(actual_size);
    output->flags = load_be32(entry + JIXIA_FFS_ENTRY_FLAGS_OFFSET);
    output->data_integrity = load_be16(entry + JIXIA_FFS_ENTRY_USER_OFFSET + 2U);
    output->version_check = entry[JIXIA_FFS_ENTRY_USER_OFFSET + 4U];
    output->misc_flags = entry[JIXIA_FFS_ENTRY_USER_OFFSET + 5U];
    return true;
}

} // namespace

bool validate() {
    ParsedHeader header = {};
    if (!parse_header(&header)) {
        return false;
    }

    const volatile uint8_t* entry = flash_at(JIXIA_PFLASH_TOC_OFFSET + JIXIA_FFS_HEADER_SIZE);
    for (uint32_t index = 0U; index < header.entry_count; ++index) {
        if (!checksum_is_zero(entry, JIXIA_FFS_ENTRY_SIZE)) {
            return false;
        }
        entry += JIXIA_FFS_ENTRY_SIZE;
    }

    return true;
}

bool find_partition(const char* name, PartitionInfo* output) {
    if (name == nullptr || output == nullptr) {
        return false;
    }

    ParsedHeader header = {};
    if (!parse_header(&header)) {
        return false;
    }

    const volatile uint8_t* entry = flash_at(JIXIA_PFLASH_TOC_OFFSET + JIXIA_FFS_HEADER_SIZE);
    for (uint32_t index = 0U; index < header.entry_count; ++index) {
        if (!checksum_is_zero(entry, JIXIA_FFS_ENTRY_SIZE)) {
            return false;
        }

        if (name_matches(entry, name)) {
            return parse_partition(entry, header, output);
        }

        entry += JIXIA_FFS_ENTRY_SIZE;
    }

    return false;
}

} // namespace jixia::microkernel::firmware_store::ffs
