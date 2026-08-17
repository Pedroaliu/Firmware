#include "platform/qemu_virt/contained_memory.h"

#include "microkernel/memory/memory_lifecycle.h"

namespace jixia::platform::qemu_virt::contained_memory {

bool flush_to_mainstore() {
    const jixia::microkernel::memory::Snapshot state = jixia::microkernel::memory::snapshot();

    if (state.domain != jixia::microkernel::memory::MemoryDomain::transitioning ||
        state.ddr != jixia::microkernel::memory::DdrState::online ||
        state.contained_flush_complete) {
        return false;
    }

    /*
     * QEMU already stores the contained window in host-backed RAM, so there
     * are no simulated dirty cache lines to copy. The fence is the v0 platform
     * commit point corresponding to POWER's cache purge/castout operation.
     */
    __asm__ volatile("fence rw, rw" ::: "memory");

    return jixia::microkernel::memory::mark_contained_flush_complete();
}

} // namespace jixia::platform::qemu_virt::contained_memory
