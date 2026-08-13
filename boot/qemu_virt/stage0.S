#include "boot/qemu_virt/pflash_layout.h"

/*
 * M00-07.01 pflash Stage0.
 *
 * QEMU's mask ROM supplies:
 *   a0 = mhartid
 *   a1 = FDT address
 * and jumps here at pflash0 base when TCG uses pflash0 + -bios none.
 *
 * This first acceptance path is deliberately single-hart. Stage0 never
 * constructs a stack and never writes its own flash-mapped code/data.
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

    /* Locate and validate the fixed v1 image header in memory-mapped flash. */
    li s0, JIXIA_QEMU_PFLASH_BASE + JIXIA_PFLASH_HEADER_OFFSET

    ld t0, JIXIA_PFLASH_HDR_MAGIC_OFFSET(s0)
    li t1, JIXIA_PFLASH_HEADER_MAGIC
    bne t0, t1, .Lheader_fail

    lw t0, JIXIA_PFLASH_HDR_VERSION_OFFSET(s0)
    li t1, JIXIA_PFLASH_HEADER_VERSION
    bne t0, t1, .Lheader_fail

    lw t0, JIXIA_PFLASH_HDR_HEADER_SIZE_OFFSET(s0)
    li t1, JIXIA_PFLASH_HEADER_SIZE
    bne t0, t1, .Lheader_fail

    ld s1, JIXIA_PFLASH_HDR_BASE_OFFSET_OFFSET(s0)
    ld s2, JIXIA_PFLASH_HDR_BASE_SIZE_OFFSET(s0)
    ld s3, JIXIA_PFLASH_HDR_BASE_LOAD_OFFSET(s0)
    ld s4, JIXIA_PFLASH_HDR_BASE_ENTRY_OFFSET(s0)

    /* Empty, overlapping, wrapping, or out-of-flash Base images are invalid. */
    beqz s2, .Lheader_fail

    li t0, JIXIA_PFLASH_BASE_IMAGE_OFFSET
    bltu s1, t0, .Lheader_fail

    li t0, JIXIA_QEMU_PFLASH_SIZE
    bgeu s1, t0, .Lheader_fail

    add t1, s1, s2
    bltu t1, s1, .Lheader_fail
    bgtu t1, t0, .Lheader_fail

    /* M00-07.01 deliberately pins contained Base identity at 0x80000000. */
    li t0, JIXIA_CONTAINED_BASE_ADDRESS
    bne s3, t0, .Lheader_fail

    li t0, JIXIA_CONTAINED_ENTRY_ADDRESS
    bne s4, t0, .Lheader_fail

    /* Translate image-relative Base offset into the pflash MMIO address. */
    li t0, JIXIA_QEMU_PFLASH_BASE
    add s1, s1, t0

    /*
     * Copy the resident Base into the QEMU backing region that M00-07 treats
     * semantically as contained EarlyMemory until DDR_ONLINE.
     *
     * Byte copies avoid imposing a payload-alignment requirement on the image
     * builder and keep the v1 header contract simple.
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

.Lheader_fail:
    la a4, .Lheader_fail_marker
    jal ra, .Luart_puts

.Lfatal_park:
    wfi
    j .Lfatal_park

.Lsecondary_park:
    wfi
    j .Lsecondary_park

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

.Lbase_transfer_marker:
    .asciz "M00_07_BASE_TRANSFER: PASS\n"

.Lheader_fail_marker:
    .asciz "M00_07_PFLASH_HEADER: FAIL\n"
