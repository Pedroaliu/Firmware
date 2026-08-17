#include "boot/common/ffs_format.h"
#include "boot/qemu_virt/pflash_layout.h"

/*
 * M00-07.01 pflash Stage0.
 *
 * QEMU's mask ROM supplies:
 *   a0 = mhartid
 *   a1 = FDT address
 * and jumps here at pflash0 base when TCG uses pflash0 + -bios none.
 *
 * Stage0 understands only the fixed FFS TOC locator plus the JXBASE partition
 * identity. It does not know the physical placement of JXBASE in advance.
 */

.section .text.boot0, "ax", @progbits

.global jixia_stage0_start
.type jixia_stage0_start, @function

jixia_stage0_start:
    /* No asynchronous M-mode work is legal during the transfer. */
    csrci mstatus, 8
    csrw mie, zero
    csrw mscratch, zero

    /* M00-07.01 keeps the pflash bring-up single-hart by contract. */
    bnez a0, .Lsecondary_park

    la a4, .Lstage0_marker
    jal ra, .Luart_puts

    /* Locate the OpenPOWER-compatible FFS v1 TOC. */
    li s0, JIXIA_QEMU_PFLASH_BASE + JIXIA_PFLASH_TOC_OFFSET

    /* The FFS header checksum is the XOR of all native 32-bit words. */
    mv t5, s0
    li t4, JIXIA_FFS_HEADER_SIZE
    jal ra, .Lxor_checksum
    bnez t6, .Ltoc_fail

    addi t5, s0, JIXIA_FFS_HDR_MAGIC_OFFSET
    jal ra, .Lload_be32
    li t0, JIXIA_FFS_MAGIC
    bne t6, t0, .Ltoc_fail

    addi t5, s0, JIXIA_FFS_HDR_VERSION_OFFSET
    jal ra, .Lload_be32
    li t0, JIXIA_FFS_VERSION
    bne t6, t0, .Ltoc_fail

    addi t5, s0, JIXIA_FFS_HDR_ENTRY_SIZE_OFFSET
    jal ra, .Lload_be32
    li t0, JIXIA_FFS_ENTRY_SIZE
    bne t6, t0, .Ltoc_fail

    addi t5, s0, JIXIA_FFS_HDR_ENTRY_COUNT_OFFSET
    jal ra, .Lload_be32
    mv s1, t6
    beqz s1, .Ltoc_fail
    li t0, JIXIA_PFLASH_MAX_FFS_ENTRIES
    bgtu s1, t0, .Ltoc_fail

    addi t5, s0, JIXIA_FFS_HDR_BLOCK_SIZE_OFFSET
    jal ra, .Lload_be32
    mv s2, t6
    li t0, JIXIA_PFLASH_BLOCK_SIZE
    bne s2, t0, .Ltoc_fail

    addi t5, s0, JIXIA_FFS_HDR_BLOCK_COUNT_OFFSET
    jal ra, .Lload_be32
    li t0, JIXIA_PFLASH_BLOCK_COUNT
    bne t6, t0, .Ltoc_fail

    addi t5, s0, JIXIA_FFS_HDR_SIZE_OFFSET
    jal ra, .Lload_be32
    mul t0, t6, s2
    li t1, JIXIA_PFLASH_TOC_SIZE
    bne t0, t1, .Ltoc_fail

    /* Ensure header + declared entries fit inside the reserved TOC region. */
    li t0, JIXIA_FFS_ENTRY_SIZE
    mul t0, t0, s1
    addi t0, t0, JIXIA_FFS_HEADER_SIZE
    li t1, JIXIA_PFLASH_TOC_SIZE
    bgtu t0, t1, .Ltoc_fail

    /* Scan FFS entries for the null-terminated partition name "JXBASE". */
    addi s3, s0, JIXIA_FFS_HEADER_SIZE
    mv s4, s1

.Lscan_entry:
    beqz s4, .Ltoc_fail

    mv t5, s3
    li t4, JIXIA_FFS_ENTRY_SIZE
    jal ra, .Lxor_checksum
    bnez t6, .Ltoc_fail

    lbu t0, 0(s3)
    li t1, 0x4a /* J */
    bne t0, t1, .Lnext_entry
    lbu t0, 1(s3)
    li t1, 0x58 /* X */
    bne t0, t1, .Lnext_entry
    lbu t0, 2(s3)
    li t1, 0x42 /* B */
    bne t0, t1, .Lnext_entry
    lbu t0, 3(s3)
    li t1, 0x41 /* A */
    bne t0, t1, .Lnext_entry
    lbu t0, 4(s3)
    li t1, 0x53 /* S */
    bne t0, t1, .Lnext_entry
    lbu t0, 5(s3)
    li t1, 0x45 /* E */
    bne t0, t1, .Lnext_entry
    lbu t0, 6(s3)
    bnez t0, .Lnext_entry

    /* Convert FFS block units into pflash-relative byte ranges. */
    addi t5, s3, JIXIA_FFS_ENTRY_BASE_OFFSET
    jal ra, .Lload_be32
    mv s1, t6

    addi t5, s3, JIXIA_FFS_ENTRY_SIZE_OFFSET
    jal ra, .Lload_be32
    mv s4, t6

    addi t5, s3, JIXIA_FFS_ENTRY_ACTUAL_OFFSET
    jal ra, .Lload_be32
    mv s5, t6
    beqz s5, .Ltoc_fail

    mul t0, s1, s2
    mul t1, s4, s2
    bgtu s5, t1, .Ltoc_fail

    li t2, JIXIA_PFLASH_DATA_OFFSET
    bltu t0, t2, .Ltoc_fail

    add t2, t0, s5
    bltu t2, t0, .Ltoc_fail
    li t3, JIXIA_QEMU_PFLASH_SIZE
    bgtu t2, t3, .Ltoc_fail

    li t1, JIXIA_QEMU_PFLASH_BASE
    add s1, t1, t0
    mv s2, s5
    li s3, JIXIA_CONTAINED_BASE_ADDRESS
    li s4, JIXIA_CONTAINED_ENTRY_ADDRESS

    la a4, .Lffs_marker
    jal ra, .Luart_puts

    /*
     * Copy the resident Base into the QEMU backing region that M00-07 treats
     * semantically as contained EarlyMemory until DDR_ONLINE.
     */
.Lcopy_base:
    beqz s2, .Lcopy_done

    lbu t0, 0(s1)
    sb t0, 0(s3)

    addi s1, s1, 1
    addi s3, s3, 1
    addi s2, s2, -1
    j .Lcopy_base

.Lcopy_done:
    fence rw, rw

    la a4, .Lbase_transfer_marker
    jal ra, .Luart_puts

    /* Preserve QEMU's a0=hartid and a1=FDT handoff into the resident Base. */
    jr s4

.Lnext_entry:
    addi s3, s3, JIXIA_FFS_ENTRY_SIZE
    addi s4, s4, -1
    j .Lscan_entry

.Ltoc_fail:
    la a4, .Ltoc_fail_marker
    jal ra, .Luart_puts

.Lfatal_park:
    wfi
    j .Lfatal_park

.Lsecondary_park:
    wfi
    j .Lsecondary_park

/* Input: t5 -> big-endian u32. Output: t6. Clobbers t0/t1. */
.Lload_be32:
    lbu t6, 0(t5)
    slli t6, t6, 24

    lbu t0, 1(t5)
    slli t0, t0, 16
    or t6, t6, t0

    lbu t0, 2(t5)
    slli t0, t0, 8
    or t6, t6, t0

    lbu t0, 3(t5)
    or t6, t6, t0
    ret

/* Input: t5 -> aligned bytes, t4=size (multiple of 4). Output: t6 XOR checksum. */
.Lxor_checksum:
    li t6, 0

.Lchecksum_next:
    beqz t4, .Lchecksum_done
    lw t0, 0(t5)
    xor t6, t6, t0
    addi t5, t5, 4
    addi t4, t4, -4
    j .Lchecksum_next

.Lchecksum_done:
    ret

/*
 * Minimal stackless 16550A string output used only for Stage0 evidence.
 * Input: a4 -> NUL-terminated string in pflash.
 */
.Luart_puts:
    li t0, JIXIA_QEMU_UART0_BASE

.Luart_next:
    lbu t1, 0(a4)
    beqz t1, .Luart_done

.Luart_wait:
    lbu t2, JIXIA_QEMU_UART_LSR_OFFSET(t0)
    andi t2, t2, JIXIA_QEMU_UART_LSR_THRE
    beqz t2, .Luart_wait

    sb t1, 0(t0)
    addi a4, a4, 1
    j .Luart_next

.Luart_done:
    ret

.size jixia_stage0_start, . - jixia_stage0_start

.section .rodata.stage0, "a", @progbits

.Lstage0_marker:
    .asciz "M00_07_PFLASH_STAGE0: PASS\n"

.Lffs_marker:
    .asciz "M00_07_PNOR_FFS: PASS\n"

.Lbase_transfer_marker:
    .asciz "M00_07_BASE_TRANSFER: PASS\n"

.Ltoc_fail_marker:
    .asciz "M00_07_PFLASH_HEADER: FAIL\n"
