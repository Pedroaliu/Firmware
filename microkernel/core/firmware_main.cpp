#include <stdint.h>

#include "microkernel/console/console.h"
#include "platform/qemu_virt/console.h"

extern "C" void jixia_console_test();
extern "C" void jixia_recoverable_trap_test();
extern "C" void jixia_machine_timer_test();
extern "C" [[noreturn]] void jixia_trap_frame_test();

namespace jixia::microkernel {

[[noreturn]] void main(uintptr_t hart_id, uintptr_t dtb_address)
{
    jixia::platform::qemu_virt::console::initialize();

    console::out
        << '\n'
        << "Jixia M00\n"
        << "hart        : " << console::hex(hart_id) << '\n'
        << "dtb         : " << console::hex(dtb_address) << '\n'
        << "microkernel : entered (codename: Mozi)\n"
        << "trap test   : recoverable exceptions and machine timer\n";

    /*
     * The console smoke test validates the new common output path first.
     * Prior milestone tests remain live regressions, and M00-04 then adds the
     * first asynchronous machine timer interrupt before the final TrapFrame
     * capture test parks the hart.
     */
    jixia_console_test();
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
