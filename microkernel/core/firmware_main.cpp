#include <stdint.h>

#include "microkernel/console/kernel_console.h"
#include "microkernel/console/printk.h"

extern "C" void jixia_kernel_print_test();
extern "C" void jixia_recoverable_trap_test();
extern "C" [[noreturn]] void jixia_trap_frame_test();

namespace jixia::microkernel {

[[noreturn]] void main(uintptr_t hart_id, uintptr_t dtb_address)
{
    kernel_console::set_uart_mirror(true);

    printk(
        "\n"
        "Jixia M00\n"
        "hart        : %p\n"
        "dtb         : %p\n"
        "microkernel : entered (codename: Mozi)\n"
        "printk      : kernel buffer + raw UART mirror\n"
        "trap test   : recoverable EBREAK and C.EBREAK\n",
        reinterpret_cast<void*>(hart_id),
        reinterpret_cast<void*>(dtb_address));

    /*
     * F00-01 validates the standalone kernel print path first. M00-03 and
     * M00-02 remain live regressions after the print-specific test.
     */
    jixia_kernel_print_test();
    jixia_recoverable_trap_test();
    jixia_trap_frame_test();
}

} // namespace jixia::microkernel

extern "C" [[noreturn]]
void jixia_microkernel_main(uintptr_t hart_id, uintptr_t dtb_address)
{
    jixia::microkernel::main(hart_id, dtb_address);
}
