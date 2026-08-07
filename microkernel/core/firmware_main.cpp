#include <stdint.h>

#include "microkernel/console/console.h"

extern "C" void jixia_platform_console_initialize();
extern "C" void jixia_console_test();
extern "C" void jixia_recoverable_trap_test();
extern "C" [[noreturn]] void jixia_trap_frame_test();

namespace jixia::microkernel {

[[noreturn]] void main(uintptr_t hart_id, uintptr_t dtb_address)
{
    jixia_platform_console_initialize();

    console::out
        << '\n'
        << "Jixia M00\n"
        << "hart        : " << console::hex(hart_id) << '\n'
        << "dtb         : " << console::hex(dtb_address) << '\n'
        << "microkernel : entered (codename: Mozi)\n"
        << "console     : router + memory + UART sinks\n"
        << "trap test   : recoverable EBREAK and C.EBREAK\n";

    /*
     * Console is a standalone foundation on top of the completed M00-03
     * baseline. Keep prior trap milestones as live regressions after the
     * Console-specific routing/memory test.
     */
    jixia_console_test();
    jixia_recoverable_trap_test();
    jixia_trap_frame_test();
}

} // namespace jixia::microkernel

extern "C" [[noreturn]]
void jixia_microkernel_main(uintptr_t hart_id, uintptr_t dtb_address)
{
    jixia::microkernel::main(hart_id, dtb_address);
}
