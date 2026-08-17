#include <stdint.h>

#include "microkernel/arch/riscv/pre_ddr_paging_test_values.h"
#include "microkernel/arch/riscv/sv39.h"
#include "microkernel/arch/riscv/trap_cause.h"
#include "microkernel/arch/riscv/trap_frame.h"
#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"
#include "microkernel/memory/flash_provider.h"
#include "microkernel/memory/memory_lifecycle.h"
#include "microkernel/memory/page_manager.h"

#ifdef JIXIA_M00_07_03_PROBE

extern "C" [[noreturn]] void jixia_m00_07_03_enter_supervisor_paging(uint64_t satp);
extern "C" char jixia_m00_07_03_completion_ecall[];
extern "C" char jixia_m00_07_03_failure_ecall[];

namespace {

using jixia::arch::riscv::ExceptionCode;
using jixia::arch::riscv::TrapCause;
using jixia::arch::riscv::TrapFrame;
using jixia::arch::riscv::Xlen;
using jixia::arch::riscv::sv39::AddressSpace;
using jixia::microkernel::memory::BackingKind;
using jixia::microkernel::memory::Snapshot;
using jixia::microkernel::memory::page_manager::Allocation;

constexpr Xlen kMstatusMppShift = 11U;
constexpr Xlen kMstatusMppMask = 0x3U << kMstatusMppShift;
constexpr Xlen kMstatusMppSupervisor = 0x1U << kMstatusMppShift;

AddressSpace g_address_space = {.root_physical_address = 0U};
uint32_t g_page_fault_count = 0U;

[[nodiscard]] bool trusted_s_trap(const TrapFrame& frame) {
    if ((frame.mstatus & kMstatusMppMask) != kMstatusMppSupervisor) {
        return false;
    }

    const jixia::microkernel::hart::HartLocal& local = jixia::microkernel::hart::current();
    const uintptr_t frame_address = reinterpret_cast<uintptr_t>(&frame);
    const uintptr_t frame_end = frame_address + sizeof(TrapFrame);

    return local.trap_active == 1U && (frame_address % TRAP_FRAME_ALIGNMENT) == 0U &&
           frame_address >= local.trap_stack_bottom && frame_end <= local.trap_stack_top;
}

void print_failure(const char* reason) {
    jixia::microkernel::printk("M00_07_PRE_DDR_PAGING: FAIL (%s)\n", reason);
}

[[noreturn]] void finish_probe(bool passed) {
    if (passed) {
        jixia::microkernel::printk("M00_07_PRE_DDR_PAGING_RESUME: PASS\n"
                                   "M00-07.03 pre-DDR flash-backed paging: PASS\n");
    } else {
        jixia::microkernel::printk("M00_07_PRE_DDR_PAGING: FAIL (pageable result)\n");
    }

    jixia::microkernel::hart::park();
}

[[nodiscard]] bool service_instruction_page_fault(TrapFrame& frame) {
    const TrapCause cause{frame.mcause};
    if (!cause.is_exception(ExceptionCode::instruction_page_fault)) {
        return false;
    }

    if (!trusted_s_trap(frame) || frame.mtval != M00_07_03_PAGEABLE_VA ||
        frame.mepc != M00_07_03_PAGEABLE_VA || g_page_fault_count != 0U) {
        print_failure("unexpected page-fault context");
        return false;
    }

    const Snapshot state = jixia::microkernel::memory::snapshot();
    if (state.domain != jixia::microkernel::memory::MemoryDomain::contained ||
        state.ddr != jixia::microkernel::memory::DdrState::offline ||
        state.ddr_allocation_enabled) {
        print_failure("DDR became visible before pre-DDR fault");
        return false;
    }

    const Allocation page = jixia::microkernel::memory::page_manager::allocate_page();
    if (!page.valid() || page.backing != BackingKind::contained ||
        jixia::microkernel::memory::backing_for(page.physical_address) != BackingKind::contained) {
        print_failure("PageManager did not return contained backing");
        return false;
    }

    if (!jixia::microkernel::memory::flash_provider::read_extended_page(0U,
                                                                        page.physical_address)) {
        print_failure("pflash Extended page read failed");
        return false;
    }

    constexpr jixia::arch::riscv::sv39::PteFlags kPageFlags =
        jixia::arch::riscv::sv39::PteFlag::read | jixia::arch::riscv::sv39::PteFlag::execute |
        jixia::arch::riscv::sv39::PteFlag::accessed;

    if (!jixia::arch::riscv::sv39::map_page_4k(g_address_space, M00_07_03_PAGEABLE_VA,
                                               page.physical_address, kPageFlags)) {
        print_failure("Sv39 page installation failed");
        return false;
    }

    ++g_page_fault_count;
    jixia::arch::riscv::sv39::fence_address(M00_07_03_PAGEABLE_VA);
    jixia::arch::riscv::sv39::fence_instruction_stream();

    jixia::microkernel::printk("M00_07_PRE_DDR_PAGE_FAULT: PASS\n"
                               "M00_07_PRE_DDR_FLASH_READ: PASS\n"
                               "M00_07_PRE_DDR_BACKING_EARLY: PASS\n");
    return true;
}

[[nodiscard]] bool service_completion_ecall(TrapFrame& frame) {
    const TrapCause cause{frame.mcause};
    if (!cause.is_exception(ExceptionCode::environment_call_from_s)) {
        return false;
    }

    if (!trusted_s_trap(frame)) {
        print_failure("untrusted completion trap");
        return false;
    }

    const uintptr_t completion_pc = reinterpret_cast<uintptr_t>(jixia_m00_07_03_completion_ecall);
    const uintptr_t failure_pc = reinterpret_cast<uintptr_t>(jixia_m00_07_03_failure_ecall);

    if (frame.mepc == completion_pc && frame.x[10] == M00_07_03_ECALL_PASS &&
        g_page_fault_count == 1U) {
        finish_probe(true);
    }

    if (frame.mepc == failure_pc || frame.x[10] == M00_07_03_ECALL_FAIL) {
        finish_probe(false);
    }

    print_failure("unexpected completion ECALL");
    return false;
}

} // namespace

extern "C" [[noreturn]] void jixia_m00_07_03_run_pre_ddr_paging_probe() {
    using namespace jixia::microkernel;

    if (!memory::validate_contained_invariants()) {
        printk("M00_07_PRE_DDR_PAGING: FAIL (contained state invalid)\n");
        hart::park();
    }

    memory::page_manager::reset();
    if (!memory::page_manager::add_contained_bootstrap_pool()) {
        printk("M00_07_PRE_DDR_PAGING: FAIL (bootstrap PageManager range)\n");
        hart::park();
    }

    g_address_space = jixia::arch::riscv::sv39::create_address_space();
    if (!g_address_space.valid()) {
        printk("M00_07_PRE_DDR_PAGING: FAIL (Sv39 root allocation)\n");
        hart::park();
    }

    const Snapshot state = memory::snapshot();
    constexpr jixia::arch::riscv::sv39::PteFlags kResidentFlags =
        jixia::arch::riscv::sv39::PteFlag::read | jixia::arch::riscv::sv39::PteFlag::write |
        jixia::arch::riscv::sv39::PteFlag::execute | jixia::arch::riscv::sv39::PteFlag::accessed |
        jixia::arch::riscv::sv39::PteFlag::dirty;

    if (!jixia::arch::riscv::sv39::map_range_4k(g_address_space, state.contained.base,
                                                state.contained.base, state.contained.size,
                                                kResidentFlags)) {
        printk("M00_07_PRE_DDR_PAGING: FAIL (resident identity map)\n");
        hart::park();
    }

    memory::flash_provider::ImageInfo image = {};
    if (!memory::flash_provider::image_info(&image) ||
        image.extended_size < memory::flash_provider::kPageSize) {
        printk("M00_07_PRE_DDR_PAGING: FAIL (Extended image absent)\n");
        hart::park();
    }

    const uint64_t satp = jixia::arch::riscv::sv39::satp_value(g_address_space);
    jixia::arch::riscv::sv39::fence_all();

    printk(
        "\n"
        "[Jixia][M00-07.03][PreDdrPaging]\n"
        "root        : %p\n"
        "extended    : [%p, %p)\n"
        "early pages : %lu\n"
        "target VA   : %p\n"
        "M00_07_PRE_DDR_PAGING_ARMED: PASS\n",
        reinterpret_cast<void*>(g_address_space.root_physical_address),
        reinterpret_cast<void*>(image.extended_flash_address),
        reinterpret_cast<void*>(image.extended_flash_address + image.extended_size),
        static_cast<unsigned long>(memory::page_manager::remaining_pages(BackingKind::contained)),
        reinterpret_cast<void*>(M00_07_03_PAGEABLE_VA));

    jixia_m00_07_03_enter_supervisor_paging(satp);
}

extern "C" bool jixia_m00_07_03_try_handle_trap(TrapFrame* frame) {
    if (frame == nullptr) {
        return false;
    }

    if (service_instruction_page_fault(*frame)) {
        return true;
    }

    return service_completion_ecall(*frame);
}

#endif
