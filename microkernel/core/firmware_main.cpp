#include <stdint.h>

#include "uart.h"

extern "C" [[noreturn]] void jixia_trap_frame_test();

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
    uart_puts("trap test   : complete integer TrapFrame\n");

    jixia_trap_frame_test();
}

} // namespace jixia::microkernel

extern "C" [[noreturn]]
void jixia_microkernel_main(uintptr_t hart_id, uintptr_t dtb_address)
{
    jixia::microkernel::main(hart_id, dtb_address);
}
