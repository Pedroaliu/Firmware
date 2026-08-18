#pragma once

#include <stddef.h>
#include <stdint.h>

#include "microkernel/arch/riscv/sv39.h"
#include "microkernel/core/singleton.h"

namespace jixia::microkernel::memory {

class VmmManager final {
  public:
    static VmmManager& instance();

    [[nodiscard]] bool initialize();
    [[nodiscard]] bool initialized() const;

    [[nodiscard]] const jixia::arch::riscv::sv39::AddressSpace& boot_address_space() const;

    [[nodiscard]] bool map_boot_task_stack(uintptr_t physical_base, size_t size);
    [[nodiscard]] bool is_boot_user_entry(uintptr_t address) const;

    /** Temporary broad PMP grant; Sv39 still limits U virtual mappings. */
    void prepare_first_user_dispatch() const;

  private:
    friend class jixia::microkernel::Singleton<VmmManager>;

    VmmManager();

    static constexpr size_t kMaxStackMappings = 16U;

    jixia::arch::riscv::sv39::AddressSpace boot_address_space_;
    uintptr_t mapped_stack_bases_[kMaxStackMappings];
    size_t mapped_stack_count_;
    uintptr_t user_text_start_;
    uintptr_t user_text_end_;
    bool initialized_;
};

} // namespace jixia::microkernel::memory
