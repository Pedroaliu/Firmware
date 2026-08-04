    .section .text.boot, "ax", @progbits

    .global _start
    .type _start, @function

/*
 * QEMU virt firmware entry:
 *
 *   a0 = hart ID
 *   a1 = device tree blob address
 *
 * M00 currently allows only hart 0 to execute the boot kernel.
 */
_start:
    /*
     * The trap handler has not been installed yet.
     * Disable all machine-mode interrupts first.
     *
     * mstatus.MIE is bit 3.
     */
    csrci mstatus, 8
    csrw  mie, zero

    /*
     * Park all secondary harts.
     *
     * No stack is available at this point, so no normal
     * C function may be called here.
     */
    bnez a0, .Lsecondary_park

    /*
     * Initialize the global pointer.
     *
     * Relaxation must be disabled because gp has not been
     * initialized before this instruction executes.
     */
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    /*
     * Initialize the boot stack.
     *
     * The RISC-V stack grows toward lower addresses.
     * __stack_top is guaranteed to be 16-byte aligned
     * by the linker script.
     */
    la sp, __stack_top

    /*
     * Clear the .bss section.
     *
     * t0 = current address
     * t1 = end address
     *
     * Equivalent C code:
     *
     * while (t0 < t1) {
     *     *(uint64_t *)t0 = 0;
     *     t0 += 8;
     * }
     */
    la t0, __bss_start
    la t1, __bss_end

.Lclear_bss:
    bgeu t0, t1, .Lbss_done
    sd zero, 0(t0)
    addi t0, t0, 8
    j .Lclear_bss

.Lbss_done:
    /*
     * a0 and a1 are still intact, so they become:
     *
     * kernel_main(hart_id, dtb_address)
     */
    call kernel_main

    /*
     * kernel_main must not return.
     *
     * If it does return unexpectedly, park the boot hart.
     * _start has no caller, so executing ret would be invalid.
     */
.Lboot_park:
    wfi
    j .Lboot_park

    /*
     * Secondary harts remain parked until a future boot stage
     * explicitly releases them.
     */
.Lsecondary_park:
    wfi
    j .Lsecondary_park

    .size _start, . - _start