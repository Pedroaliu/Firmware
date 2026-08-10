#include "platform/qemu_virt/timer.h"

#include <stdint.h>


namespace jixia::platform::qemu_virt::timer {
namespace {


inline constexpr uintptr_t clint_base =
    0x02000000UL;

inline constexpr uintptr_t mtimecmp_base =
    clint_base + 0x4000UL;

inline constexpr uintptr_t mtimecmp_stride =
    sizeof(uint64_t);

inline constexpr uintptr_t mtime =
    clint_base + 0xBFF8UL;


[[nodiscard]]
volatile uint64_t* mmio64(uintptr_t address)
{
    return reinterpret_cast<volatile uint64_t*>(address);
}


[[nodiscard]]
uintptr_t mtimecmp_address(uintptr_t hart_id)
{
    return
        mtimecmp_base +
        hart_id * mtimecmp_stride;
}


} // namespace


uint64_t read_time()
{
    return *mmio64(mtime);
}


void set_compare(
    uintptr_t hart_id,
    uint64_t deadline)
{
    *mmio64(
        mtimecmp_address(hart_id)) =
        deadline;

    __asm__ volatile(
        "fence iorw, iorw"
        ::: "memory");
}


void disarm(uintptr_t hart_id)
{
    set_compare(
        hart_id,
        UINT64_MAX);
}


} // namespace jixia::platform::qemu_virt::timer