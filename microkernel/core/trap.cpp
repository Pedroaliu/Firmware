#include <stdint.h>

#include "microkernel/arch/riscv/instruction_decode.h"
#include "microkernel/arch/riscv/privilege_transition_test_values.h"
#include "microkernel/arch/riscv/trap_cause.h"
#include "microkernel/arch/riscv/trap_frame.h"
#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"
#include "microkernel/core/timer.h"

extern "C" int jixia_trap_frame_test_is_active();
extern "C" [[noreturn]]
void jixia_trap_frame_test_finish(
    jixia::arch::riscv::TrapFrame* frame);

#ifdef JIXIA_M00_06_03_PROBE
extern "C" char jixia_m00_06_03_ecall_site[];
extern "C" char __m00_06_02_supervisor_stack_bottom[];
extern "C" char __m00_06_02_supervisor_stack_top[];
#endif

#ifdef JIXIA_M00_06_04_PROBE
extern "C" bool jixia_m00_06_04_try_handle_supervisor_ecall(jixia::arch::riscv::TrapFrame* frame);
#endif

#ifdef JIXIA_M00_07_03_PROBE
extern "C" bool jixia_m00_07_03_try_handle_trap(jixia::arch::riscv::TrapFrame* frame);
#endif

namespace jixia::microkernel::trap {

using jixia::arch::riscv::BreakpointDecode;
using jixia::arch::riscv::ExceptionCode;
using jixia::arch::riscv::InterruptCode;
using jixia::arch::riscv::TrapCause;
using jixia::arch::riscv::TrapFrame;
using jixia::arch::riscv::Xlen;
using jixia::arch::riscv::decode_breakpoint_at;
using jixia::arch::riscv::instruction_length_bytes;

[[noreturn]] void fatal(const TrapFrame& frame)
{
    const TrapCause cause{frame.mcause};
    const Xlen cause_code = cause.code();

    printk(
        "\n"
        "[Jixia][Microkernel][fatal trap]\n"
        "frame     : %p\n"
        "kind      : %s\n"
        "mstatus   : %p\n"
        "mcause    : %p\n"
        "code      : %lu\n"
        "mepc      : %p\n"
        "mtval     : %p\n"
        "saved sp  : %p\n"
        "saved ra  : %p\n",
        static_cast<void*>(const_cast<TrapFrame*>(&frame)),
        cause.is_interrupt() ? "interrupt" : "exception",
        reinterpret_cast<void*>(frame.mstatus),
        reinterpret_cast<void*>(frame.mcause),
        static_cast<unsigned long>(cause_code),
        reinterpret_cast<void*>(frame.mepc),
        reinterpret_cast<void*>(frame.mtval),
        reinterpret_cast<void*>(frame.x[2]),
        reinterpret_cast<void*>(frame.x[1]));

    for (;;)
    {
        __asm__ volatile("wfi");
    }
}

[[nodiscard]]
bool try_handle_machine_timer_interrupt(const TrapFrame& frame)
{
    const TrapCause cause{frame.mcause};

    if (!cause.is_interrupt(InterruptCode::machine_timer))
    {
        return false;
    }

    /*
     * This is an asynchronous interrupt. Unlike EBREAK/C.EBREAK, do not
     * advance frame.mepc: it already identifies the interrupted resume point.
     */
    timer::handle_interrupt();
    return true;
}

[[nodiscard]]
bool try_recover_breakpoint(TrapFrame& frame)
{
    const TrapCause cause{frame.mcause};

    if (!cause.is_exception(ExceptionCode::breakpoint))
    {
        return false;
    }

    /*
     * mcause code 3 can also describe hardware breakpoints/watchpoints.
     * Confirm that mepc really points at EBREAK or C.EBREAK before advancing
     * the saved resume PC.
     */
    const BreakpointDecode decoded =
        decode_breakpoint_at(frame.mepc);

    if (!decoded.recognized())
    {
        return false;
    }

    /*
     * EBREAK and C.EBREAK leave mepc pointing at themselves. trap.S later
     * writes this modified value back to the mepc CSR before executing mret.
     */
    frame.mepc += instruction_length_bytes(decoded.length);
    return true;
}

#ifdef JIXIA_M00_06_03_PROBE
namespace {

constexpr Xlen kMstatusMppShift = 11U;
constexpr Xlen kMstatusMppMask = 0x3U << kMstatusMppShift;
constexpr Xlen kMstatusMppSupervisor = 0x1U << kMstatusMppShift;

} // namespace

[[nodiscard]]
bool try_handle_supervisor_ecall(TrapFrame& frame) {
    const TrapCause cause{frame.mcause};

    if (!cause.is_exception(ExceptionCode::environment_call_from_s)) {
        return false;
    }

    const uintptr_t expected_ecall_pc = reinterpret_cast<uintptr_t>(jixia_m00_06_03_ecall_site);

    if (frame.mepc != expected_ecall_pc) {
        return false;
    }

    if ((frame.mstatus & kMstatusMppMask) != kMstatusMppSupervisor) {
        return false;
    }

    if (frame.x[3] != M00_06_03_GP_MARKER || frame.x[10] != M00_06_03_A0_MARKER ||
        frame.x[17] != M00_06_03_A7_MARKER) {
        return false;
    }

    /*
     * x2 is an interrupted register value, not trusted storage. It must point
     * into the S probe stack, and an unused downward-growing stack may have
     * x2 exactly equal to stack_top.
     */
    const uintptr_t supervisor_stack_bottom =
        reinterpret_cast<uintptr_t>(__m00_06_02_supervisor_stack_bottom);
    const uintptr_t supervisor_stack_top =
        reinterpret_cast<uintptr_t>(__m00_06_02_supervisor_stack_top);
    const uintptr_t saved_sp = frame.x[2];

    if (saved_sp < supervisor_stack_bottom || saved_sp > supervisor_stack_top) {
        return false;
    }

    /*
     * The privileged frame itself must live entirely on current HartLocal's
     * dedicated trusted M trap stack while the trap owns that stack.
     */
    const hart::HartLocal& local = hart::current();
    const uintptr_t frame_address = reinterpret_cast<uintptr_t>(&frame);
    const uintptr_t frame_end = frame_address + sizeof(TrapFrame);

    if ((frame_address % TRAP_FRAME_ALIGNMENT) != 0U) {
        return false;
    }

    if (frame_address < local.trap_stack_bottom || frame_end > local.trap_stack_top) {
        return false;
    }

    if (local.trap_active != 1U) {
        return false;
    }

    /* Validate first, mutate the future return context only after acceptance. */
    frame.mepc += M00_06_03_ECALL_INSTRUCTION_BYTES;
    return true;
}
#endif

void dispatch(TrapFrame& frame)
{
    if (try_handle_machine_timer_interrupt(frame))
    {
        return;
    }

    if (try_recover_breakpoint(frame))
    {
        return;
    }

#ifdef JIXIA_M00_06_03_PROBE
    if (try_handle_supervisor_ecall(frame)) {
        return;
    }
#endif

#ifdef JIXIA_M00_06_04_PROBE
    if (jixia_m00_06_04_try_handle_supervisor_ecall(&frame)) {
        return;
    }
#endif

#ifdef JIXIA_M00_07_03_PROBE
    if (jixia_m00_07_03_try_handle_trap(&frame)) {
        return;
    }
#endif

    fatal(frame);
}

} // namespace jixia::microkernel::trap

extern "C"
void jixia_trap_dispatch(jixia::arch::riscv::TrapFrame* frame)
{
    if (jixia_trap_frame_test_is_active() != 0)
    {
        jixia_trap_frame_test_finish(frame);
    }

    jixia::microkernel::trap::dispatch(*frame);
}
