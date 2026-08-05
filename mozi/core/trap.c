#include <stdint.h>

#include "uart.h"

__attribute__((noreturn))
void trap_fatal(uintptr_t cause, uintptr_t epc, uintptr_t tval)
{
    const uintptr_t interrupt_bit =
        (uintptr_t)1U << ((sizeof(uintptr_t) * 8U) - 1U);

    const uintptr_t cause_code = cause & ~interrupt_bit;

    uart_puts("\n");
    uart_puts("[mozi fatal trap]\n");

    uart_puts("kind      : ");
    if ((cause & interrupt_bit) != 0U)
    {
        uart_puts("interrupt\n");
    }
    else
    {
        uart_puts("exception\n");
    }

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
