#pragma once

#include <stdint.h>

namespace jixia::platform::qemu_virt::timer {

[[nodiscard]] uint64_t read_time();
void set_compare(uintptr_t hart_id, uint64_t deadline);
void disarm(uintptr_t hart_id);

} // namespace jixia::platform::qemu_virt::timer
