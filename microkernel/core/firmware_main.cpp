#include <stdint.h>

#include "uart.h"

namespace jixia::microkernel {

[[noreturn]] void main(uintptr_t hart_id, uintptr_t dtb_address)
{
    uart_puts("\n");
    uart_puts("Jixia M00\n");
    uart_puts("hart        : ");
    uart_put_hex_uintptr(hart_id);
    uart_puts("\n");

    uart_puts("dtb         : ");
    uart_put_hex_uintptr(dtb_address);
    uart_puts("\n");

    uart_puts("microkernel : entered (codename: Mozi)\n");
    uart_puts("trap test   : breakpoint\n");

    /* Emit an explicit 32-bit EBREAK rather than C.EBREAK. */
    __asm__ volatile(".word 0x00100073");

    for (;;)
    {
        __asm__ volatile("wfi");
    }
}

} // namespace jixia::microkernel

extern "C" [[noreturn]]
void jixia_microkernel_main(uintptr_t hart_id, uintptr_t dtb_address)
{
    jixia::microkernel::main(hart_id, dtb_address);
}
