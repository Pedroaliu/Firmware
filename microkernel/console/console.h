#pragma once

#include <stddef.h>
#include <stdint.h>

#include "microkernel/console/sink.h"

namespace jixia::microkernel::console {

enum class Route : uint8_t {
    normal,
    emergency,
};

void initialize();
[[nodiscard]] bool attach_sink(const ConsoleSink& sink);
void put(Route route, char ch);
void write(Route route, const char* text);
void flush(Route route);

[[nodiscard]] const char* memory_buffer();
[[nodiscard]] size_t memory_capacity();
[[nodiscard]] size_t memory_write_position();
[[nodiscard]] bool memory_wrapped();

struct HexValue {
    uint64_t value;
    size_t digits;
};

[[nodiscard]] constexpr HexValue hex(
    uint64_t value,
    size_t digits = sizeof(uintptr_t) * 2U)
{
    return HexValue{value, digits};
}

class ConsoleStream final
{
public:
    constexpr explicit ConsoleStream(Route route)
        : route_(route)
    {
    }

    ConsoleStream& operator<<(const char* text);
    ConsoleStream& operator<<(char ch);
    ConsoleStream& operator<<(bool value);
    ConsoleStream& operator<<(unsigned int value);
    ConsoleStream& operator<<(int value);
    ConsoleStream& operator<<(unsigned long value);
    ConsoleStream& operator<<(long value);
    ConsoleStream& operator<<(unsigned long long value);
    ConsoleStream& operator<<(long long value);
    ConsoleStream& operator<<(const void* pointer);
    ConsoleStream& operator<<(HexValue value);

private:
    Route route_;
};

inline constinit ConsoleStream out{Route::normal};
inline constinit ConsoleStream emergency{Route::emergency};

} // namespace jixia::microkernel::console
