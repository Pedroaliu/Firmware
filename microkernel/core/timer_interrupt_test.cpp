#include <stdint.h>

#include "microkernel/console/console.h"
#include "microkernel/core/timer.h"

namespace jixia::microkernel::timer_test {

void run()
{
    /*
     * QEMU virt normally exposes a 10 MHz timebase, so 100,000 ticks is about
     * 10 ms. The test depends only on the deadline being in the future, not on
     * an exact wall-clock duration.
     */
    constexpr uint64_t deadline_delta_ticks = 100000U;

    console::out << "\n[Jixia][Test][MachineTimer]\n";

    const uintptr_t before = timer::interrupt_count();
    timer::arm_once(deadline_delta_ticks);

    while (timer::interrupt_count() == before)
    {
        __asm__ volatile("wfi" ::: "memory");
    }

    /* Return the test environment to the interrupt-disabled baseline. */
    timer::disable_global_interrupts();

    const uintptr_t after = timer::interrupt_count();
    if (after != (before + 1U))
    {
        console::emergency << "MACHINE_TIMER_TEST: FAIL\n";
        for (;;)
        {
            __asm__ volatile("wfi");
        }
    }

    console::out
        << "interrupt  : machine timer observed and returned\n"
        << "MACHINE_TIMER_TEST: PASS\n";
}

} // namespace jixia::microkernel::timer_test

extern "C" void jixia_machine_timer_test()
{
    jixia::microkernel::timer_test::run();
}
