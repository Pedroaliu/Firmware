#pragma once

#include <stdint.h>

namespace jixia::platform::qemu_virt::timer {

[[nodiscard]] uint64_t read_time();
void set_compare(uint64_t deadline);
void disarm();

} // namespace jixia::platform::qemu_virt::timer
