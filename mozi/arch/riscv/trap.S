.section .text.trap, "ax", @progbits

.align 2
.global trap_entry
.type trap_entry, @function

/*
 * Minimal fatal machine-mode trap entry.
 *
 * The complete interrupted context is not preserved.
 * This handler therefore cannot resume execution.
 */
trap_entry:
    addi sp, sp, -16
    sd   ra, 8(sp)

    /* trap_fatal(mcause, mepc, mtval) */
    csrr a0, mcause
    csrr a1, mepc
    csrr a2, mtval

    call trap_fatal

    /* trap_fatal() must never return. */
.Ltrap_park:
    wfi
    j .Ltrap_park

    .size trap_entry, . - trap_entry
