#include "microkernel/core/timer.h"

#include <stdint.h>

#include "platform/qemu_virt/timer.h"

namespace jixia::microkernel::timer {
namespace {

inline constexpr uintptr_t mstatus_mie = uintptr_t{1} << 3U;
inline constexpr uintptr_t mie_mtie = uintptr_t{1} << 7U;

volatile uintptr_t machine_timer_interrupt_count = 0;

void enable_machine_timer_interrupt()
{
    __asm__ volatile("csrs mie, %0" :: "r"(mie_mtie) : "memory");
}

void disable_machine_timer_interrupt()
{
    __asm__ volatile("csrc mie, %0" :: "r"(mie_mtie) : "memory");
}

void enable_global_interrupts()
{
    __asm__ volatile("csrs mstatus, %0" :: "r"(mstatus_mie) : "memory");
}

} // namespace

void arm_once(uint64_t delta_ticks)
{
    /*
     * Program the deadline before enabling delivery. If the deadline becomes
     * pending first, the interrupt is held pending until both MTIE and MIE are
     * enabled; this avoids exposing an uninitialized compare value.
     */
    const uint64_t now = jixia::platform::qemu_virt::timer::read_time();
    jixia::platform::qemu_virt::timer::set_compare(now + delta_ticks);

    enable_machine_timer_interrupt();
    enable_global_interrupts();
}

void handle_interrupt()
{
    /*
     * MTIP is level-sensitive to the timer compare condition. Move mtimecmp
     * into the future (here: disarm it) and disable MTIE before returning,
     * otherwise mret could immediately trap again on the same condition.
     */
    disable_machine_timer_interrupt();
    jixia::platform::qemu_virt::timer::disarm();
    ++machine_timer_interrupt_count;
}

void disable_global_interrupts()
{
    __asm__ volatile("csrc mstatus, %0" :: "r"(mstatus_mie) : "memory");
}

uintptr_t interrupt_count()
{
    return machine_timer_interrupt_count;
}

} // namespace jixia::microkernel::timer
