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

    uart_puts("trap test : breakpoint\n");

    /*
     * Emit the 32-bit EBREAK instruction explicitly.
     *
     * Using a fixed 32-bit encoding avoids the assembler
     * selecting the compressed C.EBREAK instruction.
     */
    __asm__ volatile(".word 0x00100073");

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