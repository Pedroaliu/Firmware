#include "microkernel/core/vmm_manager.h"

#include "microkernel/core/singleton.h"
#include "microkernel/memory/page_manager.h"

extern "C" char __user_text_start[];
extern "C" char __user_text_end[];

namespace jixia::microkernel::memory {

VmmManager::VmmManager()
    : boot_address_space_{.root_physical_address = 0U}, mapped_stack_bases_{},
      mapped_stack_count_(0U), user_text_start_(0U), user_text_end_(0U), initialized_(false) {
}

VmmManager& VmmManager::instance() {
    return Singleton<VmmManager>::instance();
}

bool VmmManager::initialize() {
    if (initialized_) {
        return true;
    }

    user_text_start_ = reinterpret_cast<uintptr_t>(__user_text_start);
    user_text_end_ = reinterpret_cast<uintptr_t>(__user_text_end);

    if (user_text_end_ <= user_text_start_ || (user_text_start_ % page_manager::kPageSize) != 0U ||
        (user_text_end_ % page_manager::kPageSize) != 0U) {
        return false;
    }

    boot_address_space_ = jixia::arch::riscv::sv39::create_address_space();
    if (!boot_address_space_.valid()) {
        return false;
    }

    constexpr jixia::arch::riscv::sv39::PteFlags kUserTextFlags =
        jixia::arch::riscv::sv39::PteFlag::read | jixia::arch::riscv::sv39::PteFlag::execute |
        jixia::arch::riscv::sv39::PteFlag::user | jixia::arch::riscv::sv39::PteFlag::accessed;

    if (!jixia::arch::riscv::sv39::map_range_4k(boot_address_space_, user_text_start_,
                                                user_text_start_, user_text_end_ - user_text_start_,
                                                kUserTextFlags)) {
        return false;
    }

    jixia::arch::riscv::sv39::fence_all();
    jixia::arch::riscv::sv39::fence_instruction_stream();
    initialized_ = true;
    return true;
}

bool VmmManager::initialized() const {
    return initialized_;
}

const jixia::arch::riscv::sv39::AddressSpace& VmmManager::boot_address_space() const {
    return boot_address_space_;
}

bool VmmManager::map_boot_task_stack(uintptr_t physical_base, size_t size) {
    if (!initialized_ || size == 0U || (physical_base % page_manager::kPageSize) != 0U ||
        (size % page_manager::kPageSize) != 0U) {
        return false;
    }

    for (size_t index = 0U; index < mapped_stack_count_; ++index) {
        if (mapped_stack_bases_[index] == physical_base) {
            return true;
        }
    }

    if (mapped_stack_count_ >= kMaxStackMappings) {
        return false;
    }

    constexpr jixia::arch::riscv::sv39::PteFlags kUserStackFlags =
        jixia::arch::riscv::sv39::PteFlag::read | jixia::arch::riscv::sv39::PteFlag::write |
        jixia::arch::riscv::sv39::PteFlag::user | jixia::arch::riscv::sv39::PteFlag::accessed |
        jixia::arch::riscv::sv39::PteFlag::dirty;

    if (!jixia::arch::riscv::sv39::map_range_4k(boot_address_space_, physical_base, physical_base,
                                                size, kUserStackFlags)) {
        return false;
    }

    mapped_stack_bases_[mapped_stack_count_] = physical_base;
    ++mapped_stack_count_;
    jixia::arch::riscv::sv39::fence_address(physical_base);
    return true;
}

bool VmmManager::is_boot_user_entry(uintptr_t address) const {
    return initialized_ && address >= user_text_start_ && address < user_text_end_;
}

void VmmManager::prepare_first_user_dispatch() const {
    __asm__ volatile("csrci mstatus, 8\n"
                     "csrw mie, zero\n"
                     "csrw medeleg, zero\n"
                     "csrw mideleg, zero\n"
                     "li t0, -1\n"
                     "csrw pmpaddr0, t0\n"
                     "li t0, 0x1f\n"
                     "csrw pmpcfg0, t0\n"
                     :
                     :
                     : "t0", "memory");
}

} // namespace jixia::microkernel::memory
