#include <stdint.h>

#include "uart.h"

void kernel_main(uintptr_t hart_id, uintptr_t dtb_address)
{
    uart_puts("\n");
    uart_puts("ArchFW M00\n");
    uart_puts("hart      : ");
    uart_put_hex_uintptr(hart_id);
    uart_puts("\n");

    uart_puts("dtb       : ");
    uart_put_hex_uintptr(dtb_address);
    uart_puts("\n");

    uart_puts("kernel    : entered\n");

    /*
     * Interrupts are still disabled. WFI may therefore behave
     * as an implementation-defined low-power hint, so retain
     * the loop even if WFI returns immediately.
     */
    for (;;)
    {
        __asm__ volatile("wfi");
    }
}