#include <stdint.h>

#include "microkernel/console/kernel_console.h"
#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"
#include "microkernel/core/kernel.h"
#include "microkernel/core/smp_timer_test.h"
#include "microkernel/memory/memory_lifecycle.h"
#include "lib/fdt.h"


extern "C" void jixia_kernel_print_test();
extern "C" void jixia_recoverable_trap_test();
extern "C" void jixia_machine_timer_test();
extern "C" [[noreturn]] void jixia_trap_frame_test();
extern "C" [[noreturn]] void jixia_m00_06_02_enter_supervisor();
extern "C" [[noreturn]] void jixia_m00_06_03_enter_supervisor_ecall();
extern "C" [[noreturn]] void jixia_m00_06_04_enter_supervisor_boundary();
extern "C" [[noreturn]] void jixia_m00_07_03_run_pre_ddr_paging_probe();
extern "C" [[noreturn]] void jixia_m00_07_04_run_mainstore_transition_probe();
extern "C" void jixia_wait_for_executive_release();

namespace jixia::microkernel {
namespace {

void initialize_memory_foundation() {
    memory::initialize_contained();
    const memory::Snapshot state = memory::snapshot();

    printk("\n"
           "[Jixia][M00-07][Memory]\n"
           "state       : %s\n"
           "contained   : [%p, %p)\n"
           "ddr         : %s\n"
           "ddr alloc   : %s\n",
           memory::domain_name(state.domain), reinterpret_cast<void*>(state.contained.base),
           reinterpret_cast<void*>(state.contained.base + state.contained.size),
           memory::ddr_state_name(state.ddr),
           state.ddr_allocation_enabled ? "enabled" : "disabled");

    if (!memory::validate_contained_invariants()) {
        printk("M00_07_CONTAINED_MEMORY: FAIL\n");
        hart::park();
    }

    printk("M00_07_CONTAINED_MEMORY: PASS\n");
}

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

    /*
     * M00-07 begins the resident Base in an explicit contained-memory domain.
     * DDR is not allocator-visible merely because QEMU physically implements
     * the backing region as RAM.
     */
    initialize_memory_foundation();

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
        "pmp0        : permissive RWX NAPOT (probe only)\n"
        "async M irq : disabled\n"
        "M00_06_02_TRANSITION_ARMED: PASS\n");

    jixia_m00_06_02_enter_supervisor();
#elif defined(JIXIA_M00_06_03_PROBE)
    /*
     * The dedicated M00-06.03 build proves S -> M -> S through an ECALL.
     * The actual S-entry marker is emitted only after mret and after the S
     * probe has installed its own stack.
     */
    printk("\n"
           "[Jixia][M00-06.03][PrivilegeTransition]\n"
           "satp        : bare (0)\n"
           "medeleg     : 0\n"
           "mideleg     : 0\n"
           "pmp0        : permissive RWX NAPOT (probe only)\n"
           "async M irq : disabled\n"
           "M00_06_03_ECALL_ARMED: PASS\n");

    jixia_m00_06_03_enter_supervisor_ecall();
#elif defined(JIXIA_M00_06_04_PROBE)
    /*
     * M00-06.04 deliberately enters M-mode with an invalid S x2/sp value,
     * proves privileged TrapFrame storage remains on HartLocal.trap_stack,
     * then removes mscratch so a second lower-origin ECALL must fail closed.
     */
    printk("\n"
           "[Jixia][M00-06.04][PrivilegeBoundary]\n"
           "satp        : bare (0)\n"
           "medeleg     : 0\n"
           "mideleg     : 0\n"
           "pmp0        : permissive RWX NAPOT (probe only)\n"
           "async M irq : disabled\n"
           "M00_06_04_BOUNDARY_ARMED: PASS\n");

    jixia_m00_06_04_enter_supervisor_boundary();
#elif defined(JIXIA_M00_07_03_PROBE)
    /*
     * M00-07.03 leaves one executable Extended page only in pflash, enables
     * Sv39 for an S-mode probe, and lets the resident M-mode pager satisfy the
     * resulting instruction page fault from contained EarlyMemory.
     */
    jixia_m00_07_03_run_pre_ddr_paging_probe();
#elif defined(JIXIA_M00_07_04_PROBE)
    /*
     * M00-07.04 walks the explicit fake DDR lifecycle and proves that the
     * contained-to-mainstore backing transition preserves firmware address
     * identity before remaining mainstore is handed to PageManager.
     */
    jixia_m00_07_04_run_mainstore_transition_probe();
#elif defined(JIXIA_M00_08_01_PROBE)
    /*
     * Build the first Hostboot-shaped executive and dispatch a real U task.
     * This path does not call the task as an M-mode C++ function.
     */
    Kernel::instance().bootstrap(present_count);
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

    /* Preserve the established per-hart timer proof before runtime release. */
    smp_timer_test::run_secondary();

#ifdef JIXIA_M00_08_01_PROBE
    /*
     * The boot hart publishes this gate only after it has created every
     * per-hart scheduler/idle-task object. Waiting before Kernel::instance()
     * also preserves the boot-hart-first Singleton construction rule.
     */
    jixia_wait_for_executive_release();
    Kernel::instance().secondary_bootstrap();
#else
    hart::park();
#endif
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
