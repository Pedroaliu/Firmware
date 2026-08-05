    .section .text.boot, "ax", @progbits

    .global _start
    .type _start, @function

/*
 * Jixia QEMU virt entry:
 *
 *   a0 = hart ID
 *   a1 = device tree blob address
 *
 * M00 currently allows only hart 0 to execute the host firmware microkernel.
 */
_start:
    /* Disable machine-mode interrupts before installing a trap vector. */
    csrci mstatus, 8
    csrw  mie, zero

    /* No stack exists for secondary harts yet. */
    bnez a0, .Lsecondary_park

    /* Initialize gp without linker relaxation. */
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    /* Initialize the boot stack. */
    la sp, __stack_top

    /* Clear .bss. */
    la t0, __bss_start
    la t1, __bss_end

.Lclear_bss:
    bgeu t0, t1, .Lbss_done
    sd zero, 0(t0)
    addi t0, t0, 8
    j .Lclear_bss

.Lbss_done:
    /* Install the machine-mode trap vector in Direct mode. */
    la   t0, jixia_trap_entry
    csrw mtvec, t0

    /* C ABI boundary into the C++ implementation. */
    call jixia_microkernel_main

.Lboot_park:
    wfi
    j .Lboot_park

.Lsecondary_park:
    wfi
    j .Lsecondary_park

    .size _start, . - _start
