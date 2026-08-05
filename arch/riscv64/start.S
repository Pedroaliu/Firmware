.section .text.init
.align 4
.globl _start
.type _start, @function

_start:
    /* QEMU passes hart ID in a0 and FDT address in a1. */
    mv      s0, a0
    mv      s1, a1

    /* M00 is deliberately single-hart. Secondary harts are parked. */
    bnez    s0, .Lsecondary_park

    .option push
    .option norelax
    la      gp, __global_pointer$
    .option pop

    la      sp, __stack_top

    /* Clear BSS before any C code executes. */
    la      t0, __bss_start
    la      t1, __bss_end
.Lclear_bss:
    bgeu    t0, t1, .Lbss_done
    sd      zero, 0(t0)
    addi    t0, t0, 8
    j       .Lclear_bss
.Lbss_done:

    /* Install a direct-mode M-mode trap vector before entering C. */
    la      t0, archfw_trap_entry
    csrw    mtvec, t0
    csrw    mie, zero

    mv      a0, s0
    mv      a1, s1
    call    archfw_kernel_main

.Lprimary_park:
    wfi
    j       .Lprimary_park

.Lsecondary_park:
    wfi
    j       .Lsecondary_park

.size _start, . - _start
