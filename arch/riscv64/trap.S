.section .text
.align 4
.globl archfw_trap_entry
.type archfw_trap_entry, @function

archfw_trap_entry:
    csrr    a0, mcause
    csrr    a1, mepc
    csrr    a2, mtval
    call    archfw_trap_panic

.Ltrap_park:
    wfi
    j       .Ltrap_park

.size archfw_trap_entry, . - archfw_trap_entry
