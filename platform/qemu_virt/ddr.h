#pragma once

#include "microkernel/memory/memory_lifecycle.h"

namespace jixia::platform::qemu_virt::ddr {

/**
 * M00-07 QEMU platform model.
 *
 * The machine already implements RAM physically; these hooks deliberately
 * model the firmware-visible hardware lifecycle so real SPD/training/topology/
 * decode work can replace them later without collapsing the state machine.
 */
[[nodiscard]] bool discover();
[[nodiscard]] bool start_training();
[[nodiscard]] bool finish_training();
[[nodiscard]] bool build_topology();
[[nodiscard]] bool build_address_map();
[[nodiscard]] bool program_decode();
[[nodiscard]] bool online();

[[nodiscard]] jixia::microkernel::memory::PhysicalRange configured_range();

} // namespace jixia::platform::qemu_virt::ddr
