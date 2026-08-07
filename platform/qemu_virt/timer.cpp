#include "platform/qemu_virt/timer.h"

#include <stdint.h>

namespace jixia::platform::qemu_virt::timer {
namespace {

/*
 * QEMU virt CLINT-compatible machine timer layout for hart 0.
 * M00-04 is intentionally single-hart; per-hart compare selection belongs
 * to M00-05 together with per-hart state and stacks.
 */
inline constexpr uintptr_t clint_base = 0x02000000UL;
inline constexpr uintptr_t mtimecmp_hart0 = clint_base + 0x4000UL;
inline constexpr uintptr_t mtime = clint_base + 0xBFF8UL;

[[nodiscard]]
volatile uint64_t* mmio64(uintptr_t address)
{
    return reinterpret_cast<volatile uint64_t*>(address);
}

} // namespace

uint64_t read_time()
{
    return *mmio64(mtime);
}

void set_compare(uint64_t deadline)
{
    *mmio64(mtimecmp_hart0) = deadline;
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}

void disarm()
{
    set_compare(UINT64_MAX);
}

} // namespace jixia::platform::qemu_virt::timer
