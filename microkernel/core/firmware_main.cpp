#include <stdint.h>

#include "microkernel/console/kernel_console.h"
#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"
#include "microkernel/core/smp_timer_test.h"
#include "lib/fdt.h"


extern "C" void jixia_kernel_print_test();
extern "C" void jixia_recoverable_trap_test();
extern "C" void jixia_machine_timer_test();
extern "C" [[noreturn]] void jixia_trap_frame_test();
extern "C" [[noreturn]] void jixia_m00_06_02_enter_supervisor();


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
    /* Boot hart owns slot 0. */
    hart::HartLocal& boot_local = hart::initialize(
        hart_id,
        hart_index,
        hart::HartRole::boot);
    (void)boot_local;

    /*
     * Only the boot hart writes printk during M00-05.
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
     * Secondary harts publish HartLocal::state with release semantics, while
     * the boot hart observes each present slot with acquire semantics.
     */
    hart::wait_until_all_online(
        present_count);


    print_hart_table(
        present_count);

    printk(
        "SMP_POPULATION_TEST: PASS\n");


    /*
     * Every present hart now proves that its own timer compare, trap path, and
     * per-hart timer state work independently. Secondary harts still never
     * write printk; only the boot hart publishes the aggregate result.
     */
    smp_timer_test::run_boot(
        present_count);


    /* Completed foundations remain live regressions after the SMP probe. */
    jixia_kernel_print_test();
    jixia_recoverable_trap_test();
    jixia_machine_timer_test();


#ifdef JIXIA_M00_06_02_PROBE
    /*
     * The dedicated M00-06.02 build proves the one-way M -> S transition.
     * The ordinary regression build still terminates in the full TrapFrame
     * test below, so M00-02 evidence remains independently machine-checkable.
     */
    printk(
        "\n"
        "[Jixia][M00-06.02][PrivilegeTransition]\n"
        "satp        : bare (0)\n"
        "medeleg     : 0\n"
        "mideleg     : 0\n"
        "async M irq : disabled\n"
        "M00_06_02_TRANSITION_ARMED: PASS\n");

    jixia_m00_06_02_enter_supervisor();
#else
    /* Parks the boot hart after validating the saved context. */
    jixia_trap_frame_test();
#endif
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
     * Secondary harts participate only in the bounded M00-05 SMP timer probe,
     * then permanently park. This is still not arbitrary work dispatch or a
     * scheduler.
     */
    smp_timer_test::run_secondary();

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
