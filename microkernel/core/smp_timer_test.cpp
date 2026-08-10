#include "microkernel/core/smp_timer_test.h"

#include <stdint.h>

#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"
#include "microkernel/core/timer.h"


namespace jixia::microkernel::smp_timer_test {
namespace {

constexpr uint32_t kStopped = 0U;
constexpr uint32_t kStarted = 1U;

constexpr uint32_t kNotDone = 0U;
constexpr uint32_t kDone = 1U;

/*
 * QEMU virt normally uses a 10 MHz timebase. The exact wall-clock delay is
 * not part of this test; the deadline only needs to be far enough in the
 * future that every participating hart can arm its own compare register.
 */
constexpr uint64_t kTimerDeltaTicks = 100000U;

/*
 * Shared synchronization for this bounded boot-time probe.
 *
 * g_start is written only by the boot hart and read by all secondaries.
 * g_done[index] has one writer: the hart that owns dense slot index.
 */
uint32_t g_start;
uint32_t g_done[hart::kMaxHarts];


void wait_for_start()
{
    while (__atomic_load_n(
               &g_start,
               __ATOMIC_ACQUIRE) != kStarted)
    {
        __asm__ volatile("nop");
    }
}


void run_local_timer()
{
    const uintptr_t before =
        timer::interrupt_count();

    timer::arm_once(kTimerDeltaTicks);

    while (timer::interrupt_count() == before)
    {
        /*
         * The local machine timer interrupt is the event that wakes this hart.
         */
        __asm__ volatile(
            "wfi"
            ::: "memory");
    }

    /*
     * Restore the interrupt-disabled M00-05 baseline after the one-shot probe.
     */
    timer::disable_global_interrupts();

    const uintptr_t after =
        timer::interrupt_count();

    if (after != before + 1U)
    {
        hart::park();
    }
}


void publish_done(hart::HartIndex index)
{
    /*
     * The local timer count is updated before this release publication. A boot
     * hart that observes kDone with an acquire load may then consume that
     * hart's published timer state.
     */
    __atomic_store_n(
        &g_done[index],
        kDone,
        __ATOMIC_RELEASE);
}


[[nodiscard]]
bool is_done(hart::HartIndex index)
{
    return __atomic_load_n(
               &g_done[index],
               __ATOMIC_ACQUIRE)
        == kDone;
}


} // namespace


void run_secondary()
{
    hart::HartLocal& local =
        hart::current();

    /*
     * Do not enter the probe before the boot hart has initialized all test
     * state and explicitly opened the timer-test phase.
     */
    wait_for_start();

    run_local_timer();

    /*
     * Publish completion only after this hart has observed exactly one local
     * machine timer interrupt.
     */
    publish_done(local.index);
}


void run_boot(hart::HartIndex present_count)
{
    if (present_count == 0 ||
        present_count > hart::kMaxHarts)
    {
        hart::park();
    }

    const hart::HartLocal* harts =
        hart::table();

    printk(
        "\n"
        "[Jixia][Test][SmpTimer]\n");

    /*
     * BSS already starts at zero, but initialize the bounded test state
     * explicitly so the protocol is visible and self-contained.
     */
    __atomic_store_n(
        &g_start,
        kStopped,
        __ATOMIC_RELAXED);

    for (hart::HartIndex index = 0;
         index < present_count;
         ++index)
    {
        __atomic_store_n(
            &g_done[index],
            kNotDone,
            __ATOMIC_RELAXED);
    }

    /*
     * Publish the phase transition after all g_done slots are initialized.
     */
    __atomic_store_n(
        &g_start,
        kStarted,
        __ATOMIC_RELEASE);

    /*
     * The boot hart uses the same local timer path as every secondary hart.
     */
    run_local_timer();
    publish_done(hart::current().index);

    /*
     * Acquire every completion publication before consuming per-hart timer
     * state below.
     */
    for (hart::HartIndex index = 0;
         index < present_count;
         ++index)
    {
        while (!is_done(index))
        {
            __asm__ volatile("nop");
        }
    }

    bool passed = true;

    for (hart::HartIndex index = 0;
         index < present_count;
         ++index)
    {
        const hart::HartLocal& local =
            harts[index];

        const uintptr_t count =
            local.machine_timer_interrupt_count;

        printk(
            "slot %u : hart=%p timer_count=%lu\n",
            static_cast<unsigned>(local.index),
            reinterpret_cast<void*>(local.hart_id),
            static_cast<unsigned long>(count));

        if (count != 1U)
        {
            passed = false;
        }
    }

    if (!passed)
    {
        printk(
            "SMP_TIMER_TEST: FAIL\n");

        hart::park();
    }

    printk(
        "SMP_TIMER_TEST: PASS\n");
}


} // namespace jixia::microkernel::smp_timer_test
