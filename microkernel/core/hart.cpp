//
// Created by pedroa on 2026/8/10.
//

#include "microkernel/core/hart.h"


namespace {

extern "C" char __hart_stacks_start[];
extern "C" char __hart_trap_stacks_start[];


/*
 * Cleared by the boot hart before secondary harts enter C++.
 *
 * Each slot has exactly one writer:
 *
 *   g_harts[0] -> boot hart
 *   g_harts[n] -> owner of dense index n
 *
 * Other harts only observe published state.
 */
jixia::microkernel::hart::HartLocal
    g_harts[jixia::microkernel::hart::kMaxHarts];


[[noreturn]]
void halt_forever()
{
    for (;;)
    {
        __asm__ volatile("wfi");
    }
}


void bind_hart_local(
    jixia::microkernel::hart::HartLocal* local)
{
    const uintptr_t value =
        reinterpret_cast<uintptr_t>(local);

    /*
     * mscratch is the machine-mode-owned per-hart anchor.
     *
     * M00-06 trap entry uses it to reach trusted HartLocal state before it
     * touches the interrupted stack.
     */
    __asm__ volatile(
        "csrw mscratch, %0"
        :
        : "r"(value)
        : "memory");
}


} // namespace


namespace jixia::microkernel::hart {


HartLocal& initialize(
    HartId hart_id,
    HartIndex index,
    HartRole role)
{
    if (index >= kMaxHarts)
    {
        halt_forever();
    }


    HartLocal& local = g_harts[index];


    local.hart_id = hart_id;
    local.index = index;
    local.role = role;
    local.machine_timer_interrupt_count = 0U;
    local.trap_entry_sp = 0U;
    local.trap_entry_t1 = 0U;
    local.trap_active = 0U;
    local.trap_reserved = 0U;


    const uintptr_t stack_base =
        reinterpret_cast<uintptr_t>(__hart_stacks_start)
        + static_cast<uintptr_t>(index) * kBootStackSize;

    local.stack_bottom = stack_base;
    local.stack_top = stack_base + kBootStackSize;


    const uintptr_t trap_stack_base =
        reinterpret_cast<uintptr_t>(__hart_trap_stacks_start)
        + static_cast<uintptr_t>(index) * kTrapStackSize;

    local.trap_stack_bottom = trap_stack_base;
    local.trap_stack_top = trap_stack_base + kTrapStackSize;


    /*
     * Bind only after every field required by trap.S is initialized.
     */
    bind_hart_local(&local);


    /*
     * Publish the fully initialized HartLocal.
     *
     * Any hart that observes state == online with an acquire operation must
     * also observe all fields written above.
     */
    __atomic_store_n(
        &local.state,
        static_cast<uint32_t>(HartState::online),
        __ATOMIC_RELEASE);


    return local;
}


HartLocal& current()
{
    uintptr_t value = 0;


    __asm__ volatile(
        "csrr %0, mscratch"
        : "=r"(value)
        :
        : "memory");


    return *reinterpret_cast<HartLocal*>(value);
}


const HartLocal* table()
{
    return g_harts;
}


bool all_online(HartIndex expected_count)
{
    if (expected_count == 0 ||
        expected_count > kMaxHarts)
    {
        return false;
    }


    const uint32_t online =
        static_cast<uint32_t>(
            HartState::online);


    for (HartIndex index = 0;
         index < expected_count;
         ++index)
    {
        const uint32_t state =
            __atomic_load_n(
                &g_harts[index].state,
                __ATOMIC_ACQUIRE);


        if (state != online)
        {
            return false;
        }
    }


    return true;
}


void wait_until_all_online(
    HartIndex expected_count)
{
    if (expected_count == 0 ||
        expected_count > kMaxHarts)
    {
        halt_forever();
    }


    while (!all_online(expected_count))
    {
        __asm__ volatile("nop");
    }
}


[[noreturn]]
void park()
{
    halt_forever();
}


} // namespace jixia::microkernel::hart
