#pragma once

#include <stddef.h>

namespace jixia::microkernel::kernel_console {

inline constexpr size_t buffer_capacity = 36U * 1024U;

void set_uart_mirror(bool enabled);
void put(char ch);
void write(const char* text);

[[nodiscard]] const char* buffer();
[[nodiscard]] size_t size();
[[nodiscard]] bool truncated();

} // namespace jixia::microkernel::kernel_console
