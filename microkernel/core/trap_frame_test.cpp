#include <stdint.h>

#include "microkernel/arch/riscv/trap_cause.h"
#include "microkernel/arch/riscv/trap_frame.h"
#include "microkernel/arch/riscv/trap_frame_test_values.h"
#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"

extern "C" {
volatile uintptr_t jixia_trap_test_active = 0;
volatile uintptr_t jixia_trap_test_expected_sp = 0;
volatile uintptr_t jixia_trap_test_expected_gp = 0;
volatile uintptr_t jixia_trap_test_expected_tp = 0;
}

namespace jixia::microkernel::trap_test {

using jixia::arch::riscv::ExceptionCode;
using jixia::arch::riscv::TrapCause;
using jixia::arch::riscv::TrapFrame;

struct ExpectedGpr {
    uintptr_t index;
    uintptr_t value;
};

constexpr ExpectedGpr fixed_gprs[] = {
#define EXPECTED_GPR(index, reg, value) \
    {static_cast<uintptr_t>(index), static_cast<uintptr_t>(value)},
    TRAP_TEST_FIXED_GPRS(EXPECTED_GPR)
#undef EXPECTED_GPR
};

bool check_value(
    const char* field,
    uintptr_t expected,
    uintptr_t actual)
{
    if (expected == actual)
    {
        return true;
    }

    printk(
        "mismatch  : %s\n"
        "expected  : %p\n"
        "actual    : %p\n",
        field,
        reinterpret_cast<void*>(expected),
        reinterpret_cast<void*>(actual));
    return false;
}

bool check_gpr(
    uintptr_t index,
    uintptr_t expected,
    uintptr_t actual)
{
    if (expected == actual)
    {
        return true;
    }

    printk(
        "gpr index : %lu\n"
        "expected  : %p\n"
        "actual    : %p\n",
        static_cast<unsigned long>(index),
        reinterpret_cast<void*>(expected),
        reinterpret_cast<void*>(actual));
    return false;
}

[[noreturn]] void finish(const TrapFrame& frame)
{
    const TrapCause cause{frame.mcause};
    const hart::HartLocal& local = hart::current();
    const uintptr_t frame_address =
        reinterpret_cast<uintptr_t>(&frame);
    const uintptr_t frame_end =
        frame_address + sizeof(TrapFrame);

    const bool frame_on_trap_stack =
        frame_address >= local.trap_stack_bottom &&
        frame_end <= local.trap_stack_top;

    printk("\n[Jixia][Test][TrapFrame]\n");

    bool passed = true;

    passed &= check_value(
        "frame alignment",
        0U,
        frame_address % TRAP_FRAME_ALIGNMENT);
    passed &= check_value(
        "trusted trap stack",
        1U,
        frame_on_trap_stack ? 1U : 0U);
    passed &= check_value(
        "trap active",
        1U,
        static_cast<uintptr_t>(local.trap_active));
    passed &= check_value("x0", 0U, frame.x[0]);
    passed &= check_value(
        "saved sp",
        jixia_trap_test_expected_sp,
        frame.x[2]);
    passed &= check_value(
        "saved gp",
        jixia_trap_test_expected_gp,
        frame.x[3]);
    passed &= check_value(
        "saved tp",
        jixia_trap_test_expected_tp,
        frame.x[4]);

    for (const ExpectedGpr& expected : fixed_gprs)
    {
        passed &= check_gpr(
            expected.index,
            expected.value,
            frame.x[expected.index]);
    }

    passed &= check_value(
        "trap kind",
        1U,
        cause.is_exception() ? 1U : 0U);
    passed &= check_value(
        "mcause code",
        static_cast<uintptr_t>(ExceptionCode::breakpoint),
        cause.code());
    passed &= check_value("mtval", 0U, frame.mtval);

    printk(
        "%s",
        passed
            ? "TRAP_FRAME_TEST: PASS\n"
            : "TRAP_FRAME_TEST: FAIL\n");

    for (;;)
    {
        __asm__ volatile("wfi");
    }
}

} // namespace jixia::microkernel::trap_test

extern "C" int jixia_trap_frame_test_is_active()
{
    return jixia_trap_test_active != 0U ? 1 : 0;
}

extern "C" [[noreturn]]
void jixia_trap_frame_test_finish(
    jixia::arch::riscv::TrapFrame* frame)
{
    jixia::microkernel::trap_test::finish(*frame);
}
