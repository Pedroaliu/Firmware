#pragma once

#include <stdint.h>

/*
 * Jixia uses the OpenPOWER FFS v1 on-flash partition-table format.
 *
 * FFS means FSP Flash Structure. It is a firmware partition table, not a
 * POSIX filesystem. Multi-byte integers in the on-flash format are big-endian.
 */

#define JIXIA_FFS_MAGIC 0x50415254
#define JIXIA_FFS_VERSION 1
#define JIXIA_FFS_PART_NAME_MAX 15
#define JIXIA_FFS_HEADER_SIZE 48
#define JIXIA_FFS_ENTRY_SIZE 128
#define JIXIA_FFS_PID_TOPLEVEL 0xFFFFFFFF

#define JIXIA_FFS_HDR_MAGIC_OFFSET 0
#define JIXIA_FFS_HDR_VERSION_OFFSET 4
#define JIXIA_FFS_HDR_SIZE_OFFSET 8
#define JIXIA_FFS_HDR_ENTRY_SIZE_OFFSET 12
#define JIXIA_FFS_HDR_ENTRY_COUNT_OFFSET 16
#define JIXIA_FFS_HDR_BLOCK_SIZE_OFFSET 20
#define JIXIA_FFS_HDR_BLOCK_COUNT_OFFSET 24
#define JIXIA_FFS_HDR_CHECKSUM_OFFSET 44

#define JIXIA_FFS_ENTRY_NAME_OFFSET 0
#define JIXIA_FFS_ENTRY_BASE_OFFSET 16
#define JIXIA_FFS_ENTRY_SIZE_OFFSET 20
#define JIXIA_FFS_ENTRY_PID_OFFSET 24
#define JIXIA_FFS_ENTRY_ID_OFFSET 28
#define JIXIA_FFS_ENTRY_TYPE_OFFSET 32
#define JIXIA_FFS_ENTRY_FLAGS_OFFSET 36
#define JIXIA_FFS_ENTRY_ACTUAL_OFFSET 40
#define JIXIA_FFS_ENTRY_USER_OFFSET 60
#define JIXIA_FFS_ENTRY_CHECKSUM_OFFSET 124

#define JIXIA_FFS_TYPE_DATA 1
#define JIXIA_FFS_TYPE_LOGICAL 2
#define JIXIA_FFS_TYPE_PARTITION 3

#define JIXIA_FFS_FLAGS_PROTECTED 0x0001

#define JIXIA_FFS_INTEG_ECC 0x8000
#define JIXIA_FFS_VERCHECK_SHA512V 0x80
#define JIXIA_FFS_VERCHECK_SHA512EC 0x40

#define JIXIA_FFS_MISC_PRESERVED 0x80
#define JIXIA_FFS_MISC_READONLY 0x40
#define JIXIA_FFS_MISC_BACKUP 0x20
#define JIXIA_FFS_MISC_REPROVISION 0x10
#define JIXIA_FFS_MISC_VOLATILE 0x08
#define JIXIA_FFS_MISC_CLEARECC 0x04
#define JIXIA_FFS_MISC_GOLDEN 0x01

#ifndef __ASSEMBLER__
namespace jixia::firmware_store::ffs {

struct __attribute__((packed)) OnFlashHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_count;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t reserved[4];
    uint32_t checksum;
};

struct __attribute__((packed)) OnFlashEntryUser {
    uint8_t chip;
    uint8_t compression_type;
    uint16_t data_integrity;
    uint8_t version_check;
    uint8_t misc_flags;
    uint8_t free_misc[2];
    uint32_t reserved[14];
};

struct __attribute__((packed)) OnFlashEntry {
    char name[JIXIA_FFS_PART_NAME_MAX + 1];
    uint32_t base;
    uint32_t size;
    uint32_t pid;
    uint32_t id;
    uint32_t type;
    uint32_t flags;
    uint32_t actual;
    uint32_t reserved[4];
    OnFlashEntryUser user;
    uint32_t checksum;
};

static_assert(sizeof(OnFlashHeader) == JIXIA_FFS_HEADER_SIZE);
static_assert(sizeof(OnFlashEntry) == JIXIA_FFS_ENTRY_SIZE);

} // namespace jixia::firmware_store::ffs
#endif
