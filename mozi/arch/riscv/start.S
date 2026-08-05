    .section .text.boot, "ax", @progbits

    .global _start
    .type _start, @function

/*
 * Jixia QEMU virt entry:
 *
 *   a0 = hart ID
 *   a1 = device tree blob address
 *
 * M00 currently allows only hart 0 to execute the Mozi microkernel.
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
     * Install the machine-mode trap vector.
     *
     * trap_entry is four-byte aligned, therefore the low
     * two mtvec mode bits are zero and Direct mode is used.
     */
    la   t0, trap_entry
    csrw mtvec, t0

    /*
     * a0 and a1 still contain:
     *
     * a0 = hart ID
     * a1 = device tree address
     */
    call mozi_main

    /*
     * mozi_main must not return.
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
