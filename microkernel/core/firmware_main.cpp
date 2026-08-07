#include <stdint.h>

#include "uart.h"

extern "C" void jixia_recoverable_trap_test();
extern "C" void jixia_machine_timer_test();
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
    uart_puts("trap test   : recoverable exceptions and machine timer\n");

    /*
     * Keep prior milestones as live regressions. M00-04 adds the first
     * asynchronous interrupt between the recoverable-breakpoint test and the
     * final TrapFrame capture test, which parks the hart after validation.
     */
    jixia_recoverable_trap_test();
    jixia_machine_timer_test();
    jixia_trap_frame_test();
}

} // namespace jixia::microkernel

extern "C" [[noreturn]]
void jixia_microkernel_main(uintptr_t hart_id, uintptr_t dtb_address)
{
    jixia::microkernel::main(hart_id, dtb_address);
}
