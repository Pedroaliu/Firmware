#include <stdint.h>

#include "microkernel/arch/riscv/instruction_decode.h"
#include "microkernel/arch/riscv/trap_cause.h"
#include "microkernel/arch/riscv/trap_frame.h"
#include "microkernel/console/printk.h"
#include "microkernel/core/timer.h"

extern "C" int jixia_trap_frame_test_is_active();
extern "C" [[noreturn]]
void jixia_trap_frame_test_finish(
    jixia::arch::riscv::TrapFrame* frame);

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
