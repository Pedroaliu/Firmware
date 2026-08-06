#include <stdint.h>

#include "microkernel/arch/riscv/trap_frame.h"
#include "uart.h"

extern "C" int jixia_trap_frame_test_is_active();
extern "C" [[noreturn]]
void jixia_trap_frame_test_finish(
    jixia::arch::riscv::TrapFrame* frame);

namespace jixia::microkernel::trap {

using jixia::arch::riscv::TrapFrame;

[[noreturn]] void fatal(const TrapFrame& frame)
{
    constexpr uintptr_t interrupt_bit =
        uintptr_t{1} << ((sizeof(uintptr_t) * 8U) - 1U);

    const uintptr_t cause_code = frame.mcause & ~interrupt_bit;

    uart_puts("\n");
    uart_puts("[Jixia][Microkernel][fatal trap]\n");

    uart_puts("frame     : ");
    uart_put_hex_uintptr(reinterpret_cast<uintptr_t>(&frame));
    uart_puts("\n");

    uart_puts("kind      : ");
    uart_puts(
        (frame.mcause & interrupt_bit) != 0U
            ? "interrupt\n"
            : "exception\n");

    uart_puts("mstatus   : ");
    uart_put_hex_uintptr(frame.mstatus);
    uart_puts("\n");

    uart_puts("mcause    : ");
    uart_put_hex_uintptr(frame.mcause);
    uart_puts("\n");

    uart_puts("code      : ");
    uart_put_hex_uintptr(cause_code);
    uart_puts("\n");

    uart_puts("mepc      : ");
    uart_put_hex_uintptr(frame.mepc);
    uart_puts("\n");

    uart_puts("mtval     : ");
    uart_put_hex_uintptr(frame.mtval);
    uart_puts("\n");

    uart_puts("saved sp  : ");
    uart_put_hex_uintptr(frame.x[2]);
    uart_puts("\n");

    uart_puts("saved ra  : ");
    uart_put_hex_uintptr(frame.x[1]);
    uart_puts("\n");

    for (;;)
    {
        __asm__ volatile("wfi");
    }
}

} // namespace jixia::microkernel::trap

extern "C"
void jixia_trap_dispatch(jixia::arch::riscv::TrapFrame* frame)
{
    if (jixia_trap_frame_test_is_active() != 0)
    {
        jixia_trap_frame_test_finish(frame);
    }

    jixia::microkernel::trap::fatal(*frame);
}
