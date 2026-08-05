.section .text.trap, "ax", @progbits

.align 2
.global jixia_trap_entry
.type jixia_trap_entry, @function

/*
 * Minimal fatal machine-mode trap entry.
 *
 * The complete interrupted context is not preserved.
 * This handler therefore cannot resume execution.
 */
jixia_trap_entry:
    addi sp, sp, -16
    sd   ra, 8(sp)

    /* jixia_trap_fatal(mcause, mepc, mtval) */
    csrr a0, mcause
    csrr a1, mepc
    csrr a2, mtval

    call jixia_trap_fatal

.Ltrap_park:
    wfi
    j .Ltrap_park

    .size jixia_trap_entry, . - jixia_trap_entry
