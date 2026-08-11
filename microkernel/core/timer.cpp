#include "microkernel/core/timer.h"
#include "microkernel/core/hart.h"
#include "platform/qemu_virt/timer.h"

#include <stdint.h>


namespace jixia::microkernel::timer {
namespace {

inline constexpr uintptr_t mstatus_mie = uintptr_t{1} << 3U;
inline constexpr uintptr_t mie_mtie = uintptr_t{1} << 7U;


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
    hart::HartLocal& local =
        hart::current();

    const uint64_t now =
        jixia::platform::qemu_virt::timer::read_time();

    jixia::platform::qemu_virt::timer::set_compare(
        local.hart_id,
        now + delta_ticks);

    enable_machine_timer_interrupt();
    enable_global_interrupts();
}


void handle_interrupt()
{
    hart::HartLocal& local =
        hart::current();

    disable_machine_timer_interrupt();

    jixia::platform::qemu_virt::timer::disarm(
        local.hart_id);

    const uintptr_t next_count =
        local.machine_timer_interrupt_count + 1U;

    local.machine_timer_interrupt_count =
        next_count;
}


void disable_global_interrupts()
{
    __asm__ volatile("csrc mstatus, %0" :: "r"(mstatus_mie) : "memory");
}


uintptr_t interrupt_count()
{
    return hart::current().machine_timer_interrupt_count;
}


} // namespace jixia::microkernel::timer
