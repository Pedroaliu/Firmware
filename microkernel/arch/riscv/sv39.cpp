#include "microkernel/arch/riscv/sv39.h"

#include "microkernel/memory/page_manager.h"

namespace jixia::arch::riscv::sv39 {
namespace {

using jixia::microkernel::memory::page_manager::Allocation;

constexpr size_t kEntriesPerTable = kPageSize / sizeof(uint64_t);
constexpr uint64_t kPpnMask = (1ULL << 44U) - 1ULL;
constexpr uint64_t kLeafMask = PteFlag::read | PteFlag::write | PteFlag::execute;

[[nodiscard]] bool is_page_aligned(uintptr_t value) {
    return (value & (kPageSize - 1U)) == 0U;
}

[[nodiscard]] size_t vpn_index(uintptr_t virtual_address, unsigned level) {
    const unsigned shift = 12U + (9U * level);
    return static_cast<size_t>((virtual_address >> shift) & 0x1ffU);
}

[[nodiscard]] uint64_t make_pte(uintptr_t physical_address, PteFlags flags) {
    const uint64_t ppn = static_cast<uint64_t>(physical_address >> 12U);
    return (ppn << 10U) | flags;
}

[[nodiscard]] uintptr_t pte_physical_address(uint64_t pte) {
    const uint64_t ppn = (pte >> 10U) & kPpnMask;
    return static_cast<uintptr_t>(ppn << 12U);
}

[[nodiscard]] bool is_valid(uint64_t pte) {
    return (pte & PteFlag::valid) != 0U;
}

[[nodiscard]] bool is_leaf(uint64_t pte) {
    return (pte & kLeafMask) != 0U;
}

[[nodiscard]] uint64_t* table_at(uintptr_t physical_address) {
    return reinterpret_cast<uint64_t*>(physical_address);
}

[[nodiscard]] uintptr_t ensure_child_table(uint64_t& parent_pte) {
    if (is_valid(parent_pte)) {
        if (is_leaf(parent_pte)) {
            return 0U;
        }
        return pte_physical_address(parent_pte);
    }

    const Allocation allocation = jixia::microkernel::memory::page_manager::allocate_page();
    if (!allocation.valid()) {
        return 0U;
    }

    parent_pte = make_pte(allocation.physical_address, PteFlag::valid);
    return allocation.physical_address;
}

} // namespace

AddressSpace create_address_space() {
    const Allocation allocation = jixia::microkernel::memory::page_manager::allocate_page();
    if (!allocation.valid()) {
        return {.root_physical_address = 0U};
    }

    return {.root_physical_address = allocation.physical_address};
}

bool map_page_4k(const AddressSpace& address_space, uintptr_t virtual_address,
                 uintptr_t physical_address, PteFlags flags) {
    if (!address_space.valid() || !is_page_aligned(virtual_address) ||
        !is_page_aligned(physical_address)) {
        return false;
    }

    if ((flags & PteFlag::write) != 0U && (flags & PteFlag::read) == 0U) {
        return false;
    }

    uint64_t* level2 = table_at(address_space.root_physical_address);
    uintptr_t level1_address = ensure_child_table(level2[vpn_index(virtual_address, 2U)]);
    if (level1_address == 0U) {
        return false;
    }

    uint64_t* level1 = table_at(level1_address);
    uintptr_t level0_address = ensure_child_table(level1[vpn_index(virtual_address, 1U)]);
    if (level0_address == 0U) {
        return false;
    }

    uint64_t* level0 = table_at(level0_address);
    uint64_t& leaf = level0[vpn_index(virtual_address, 0U)];
    if (is_valid(leaf)) {
        return false;
    }

    leaf = make_pte(physical_address, flags | PteFlag::valid);
    return true;
}

bool map_range_4k(const AddressSpace& address_space, uintptr_t virtual_base,
                  uintptr_t physical_base, size_t size, PteFlags flags) {
    if (size == 0U || !is_page_aligned(virtual_base) || !is_page_aligned(physical_base) ||
        (size % kPageSize) != 0U) {
        return false;
    }

    const uintptr_t virtual_end = virtual_base + size;
    const uintptr_t physical_end = physical_base + size;
    if (virtual_end <= virtual_base || physical_end <= physical_base) {
        return false;
    }

    for (size_t offset = 0U; offset < size; offset += kPageSize) {
        if (!map_page_4k(address_space, virtual_base + offset, physical_base + offset, flags)) {
            return false;
        }
    }

    return true;
}

uint64_t satp_value(const AddressSpace& address_space) {
    if (!address_space.valid()) {
        return 0U;
    }

    return kSatpModeSv39 | static_cast<uint64_t>(address_space.root_physical_address >> 12U);
}

void fence_all() {
    __asm__ volatile("sfence.vma zero, zero" ::: "memory");
}

void fence_address(uintptr_t virtual_address) {
    __asm__ volatile("sfence.vma %0, zero" : : "r"(virtual_address) : "memory");
}

void fence_instruction_stream() {
    __asm__ volatile("fence.i" ::: "memory");
}

} // namespace jixia::arch::riscv::sv39
