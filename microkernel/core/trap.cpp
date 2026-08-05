#include <stdint.h>

#include "uart.h"
#include "microkernel/arch/riscv/trap_frame.h"

namespace jixia::microkernel::trap {

[[noreturn]] void fatal(
    uintptr_t cause,
    uintptr_t epc,
    uintptr_t tval)
{
    constexpr uintptr_t interrupt_bit =
        uintptr_t{1} << ((sizeof(uintptr_t) * 8U) - 1U);

    const uintptr_t cause_code = cause & ~interrupt_bit;

    uart_puts("\n");
    uart_puts("[Jixia][Microkernel][fatal trap]\n");

    uart_puts("kind      : ");
    uart_puts((cause & interrupt_bit) != 0U ? "interrupt\n" : "exception\n");

    uart_puts("mcause    : ");
    uart_put_hex_uintptr(cause);
    uart_puts("\n");

    uart_puts("code      : ");
    uart_put_hex_uintptr(cause_code);
    uart_puts("\n");

    uart_puts("mepc      : ");
    uart_put_hex_uintptr(epc);
    uart_puts("\n");

    uart_puts("mtval     : ");
    uart_put_hex_uintptr(tval);
    uart_puts("\n");

    for (;;)
    {
        __asm__ volatile("wfi");
    }
}

} // namespace jixia::microkernel::trap

extern "C" [[noreturn]]
void jixia_trap_fatal(
    uintptr_t cause,
    uintptr_t epc,
    uintptr_t tval)
{
    jixia::microkernel::trap::fatal(cause, epc, tval);
}
