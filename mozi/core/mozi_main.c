#include <stdint.h>

#include "uart.h"

void mozi_main(uintptr_t hart_id, uintptr_t dtb_address)
{
    uart_puts("\n");
    uart_puts("Jixia M00\n");
    uart_puts("hart      : ");
    uart_put_hex_uintptr(hart_id);
    uart_puts("\n");

    uart_puts("dtb       : ");
    uart_put_hex_uintptr(dtb_address);
    uart_puts("\n");

    uart_puts("mozi      : entered\n");
    uart_puts("trap test : breakpoint\n");

    /*
     * Emit the 32-bit EBREAK instruction explicitly.
     *
     * Using a fixed 32-bit encoding avoids the assembler
     * selecting the compressed C.EBREAK instruction.
     */
    __asm__ volatile(".word 0x00100073");

    for (;;)
    {
        __asm__ volatile("wfi");
    }
}
