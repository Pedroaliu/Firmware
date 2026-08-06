#include "microkernel/arch/riscv/trap_frame_test_values.h"

.section .text.trap_test, "ax", @progbits

.balign 4
.global jixia_trap_frame_test
.type jixia_trap_frame_test, @function

/*
 * Load distinct 64-bit values into every synthetic test register and trigger
 * an explicit 32-bit EBREAK. This function never returns normally.
 */
jixia_trap_frame_test:
    /* Snapshot live values that must remain valid while C++ handles the trap. */
    la t0, jixia_trap_test_expected_sp
    sd sp, 0(t0)

    la t0, jixia_trap_test_expected_gp
    sd gp, 0(t0)

    la t0, jixia_trap_test_expected_tp
    sd tp, 0(t0)

    la t0, jixia_trap_test_active
    li t1, 1
    sd t1, 0(t0)

#define LOAD_TEST_GPR(index, reg, value) li reg, value;
    TRAP_TEST_FIXED_GPRS(LOAD_TEST_GPR)
#undef LOAD_TEST_GPR

    /* Explicit 32-bit EBREAK: expected mcause=3 and mtval=0. */
    .word 0x00100073

.Ltrap_test_unexpected_return:
    wfi
    j .Ltrap_test_unexpected_return

.size jixia_trap_frame_test, . - jixia_trap_frame_test
