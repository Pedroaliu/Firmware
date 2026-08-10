#include <stdint.h>

#include "microkernel/console/kernel_console.h"
#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"
#include "lib/fdt.h"


extern "C" void jixia_kernel_print_test();
extern "C" void jixia_recoverable_trap_test();
extern "C" void jixia_machine_timer_test();
extern "C" [[noreturn]] void jixia_trap_frame_test();


namespace jixia::microkernel {
namespace {


void print_hart_table(hart::HartIndex present_count)
{
    const hart::HartLocal* harts = hart::table();


    printk(
        "\n"
        "[Jixia][M00-05][SMP]\n");


    for (hart::HartIndex index = 0;
         index < present_count;
         ++index)
    {
        const hart::HartLocal& local = harts[index];


        const char* role =
            local.role == hart::HartRole::boot
                ? "boot"
                : "secondary";


        printk(
            "slot %u : hart=%p role=%s stack=[%p, %p)\n",
            static_cast<unsigned>(local.index),
            reinterpret_cast<void*>(local.hart_id),
            role,
            reinterpret_cast<void*>(local.stack_bottom),
            reinterpret_cast<void*>(local.stack_top));
    }


    printk(
        "SMP_FOUNDATION_TEST: PASS\n");
}


} // namespace


[[noreturn]]
void boot_main(
    uintptr_t hart_id,
    uintptr_t dtb_address,
    hart::HartIndex hart_index)
{
    /*
     * Boot hart owns slot 0.
     */
    hart::HartLocal& boot_local = hart::initialize(
        hart_id,
        hart_index,
        hart::HartRole::boot);
    (void)boot_local;

    /*
     * Only the boot hart writes printk during M00-05.
     *
     * Therefore enabling the UART mirror here does not introduce concurrent
     * console writers.
     */
    kernel_console::set_uart_mirror(true);

    const ::jixia::fdt::CpuCountResult cpu_result =
        ::jixia::fdt::cpu_count(dtb_address);


    if (!cpu_result.valid)
    {
        printk(
            "smp         : invalid DTB\n"
            "SMP_POPULATION_TEST: FAIL\n");

        hart::park();
    }


    if (cpu_result.count == 0 ||
        cpu_result.count > hart::kMaxHarts)
    {
        printk(
            "smp         : unsupported CPU count %u "
            "(capacity %u)\n"
            "SMP_POPULATION_TEST: FAIL\n",
            static_cast<unsigned>(
                cpu_result.count),
            static_cast<unsigned>(
                hart::kMaxHarts));

        hart::park();
    }


    const auto present_count =
        static_cast<hart::HartIndex>(
            cpu_result.count);

    printk(
        "\n"
        "Jixia M00\n"
        "boot hart   : %p\n"
        "boot slot   : %u\n"
        "dtb         : %p\n"
        "microkernel : entered (codename: Mozi)\n"
        "smp capacity: %u hart(s)\n"
        "smp present : %u hart(s)\n",
        reinterpret_cast<void*>(hart_id),
        static_cast<unsigned>(hart_index),
        reinterpret_cast<void*>(dtb_address),
        static_cast<unsigned>(hart::kMaxHarts),
        static_cast<unsigned>(present_count));
    /*
     * Secondary harts never printk.
     *
     * They publish HartLocal::state with release semantics, while the boot
     * hart observes it with acquire semantics.
     */
    hart::wait_until_all_online(
        present_count);


    print_hart_table(
        present_count);

    printk(
        "SMP_POPULATION_TEST: PASS\n");


    /*
     * Completed foundations remain live regressions.
     *
     * M00-04 currently programs hart0's mtimecmp, so the boot role remains
     * architectural hart 0 during this milestone.
     */
    jixia_kernel_print_test();
    jixia_recoverable_trap_test();
    jixia_machine_timer_test();


    /*
     * Parks the boot hart after validating the saved context.
     */
    jixia_trap_frame_test();
}


[[noreturn]]
void secondary_main(
    uintptr_t hart_id,
    uintptr_t dtb_address,
    hart::HartIndex hart_index)
{
    (void)dtb_address;


    hart::HartLocal& secondary_local = hart::initialize(
        hart_id,
        hart_index,
        hart::HartRole::secondary);
    (void)secondary_local;

    /*
     * M00-05 proves execution ownership and rendezvous only.
     *
     * Secondary harts do not run arbitrary firmware work yet. Scheduler/task
     * dispatch comes later.
     */
    hart::park();
}


} // namespace jixia::microkernel


extern "C" [[noreturn]]
void jixia_microkernel_boot_main(
    uintptr_t hart_id,
    uintptr_t dtb_address,
    uint32_t hart_index)
{
    jixia::microkernel::boot_main(
        hart_id,
        dtb_address,
        hart_index);
}


extern "C" [[noreturn]]
void jixia_microkernel_secondary_main(
    uintptr_t hart_id,
    uintptr_t dtb_address,
    uint32_t hart_index)
{
    jixia::microkernel::secondary_main(
        hart_id,
        dtb_address,
        hart_index);
}