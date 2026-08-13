#pragma once

namespace jixia::platform::qemu_virt::contained_memory {

/**
 * Complete the platform-specific contained-cache flush/castout phase.
 *
 * QEMU virt does not expose a POWER-style backing-cache mode, so M00-07 models
 * this as an ordered semantic handoff. Jingjie/SimSoc can later replace this
 * hook with real dirty-line writeback while preserving the upper contract.
 */
[[nodiscard]] bool flush_to_mainstore();

} // namespace jixia::platform::qemu_virt::contained_memory
