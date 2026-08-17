#include "platform/qemu_virt/ddr.h"

namespace jixia::platform::qemu_virt::ddr {
namespace {

constexpr uintptr_t kQemuDdrBase = 0x80000000ULL;
constexpr size_t kQemuDdrSize = 128U * 1024U * 1024U;

} // namespace

jixia::microkernel::memory::PhysicalRange configured_range() {
    return {.base = kQemuDdrBase, .size = kQemuDdrSize};
}

bool discover() {
    return jixia::microkernel::memory::ddr_mark_discovered(configured_range());
}

bool start_training() {
    return jixia::microkernel::memory::ddr_begin_training();
}

bool finish_training() {
    /* QEMU v0 has no DDR PHY; completion is an explicit stub boundary. */
    __asm__ volatile("fence rw, rw" ::: "memory");
    return jixia::microkernel::memory::ddr_mark_trained();
}

bool build_topology() {
    const jixia::microkernel::memory::Snapshot state = jixia::microkernel::memory::snapshot();

    /*
     * Topology is deliberately a no-op in QEMU v0, but it remains a named
     * stage between training and PA layout so Hostboot-style discovery/
     * grouping can replace it later.
     */
    return state.domain == jixia::microkernel::memory::MemoryDomain::contained &&
           state.ddr == jixia::microkernel::memory::DdrState::trained;
}

bool build_address_map() {
    return jixia::microkernel::memory::ddr_mark_address_map_ready();
}

bool program_decode() {
    /* QEMU already decodes RAM; retain an ordering fence as the commit point. */
    __asm__ volatile("fence rw, rw" ::: "memory");
    return jixia::microkernel::memory::ddr_mark_decode_committed();
}

bool online() {
    return jixia::microkernel::memory::ddr_mark_online();
}

} // namespace jixia::platform::qemu_virt::ddr
