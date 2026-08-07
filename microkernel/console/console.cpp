#include "microkernel/console/console.h"

#include <stddef.h>
#include <stdint.h>

namespace jixia::microkernel::console {
namespace {

inline constexpr size_t max_sinks = 8U;
inline constexpr size_t memory_sink_size = 36U * 1024U;

class ConsoleRouter final
{
public:
    void reset()
    {
        for (size_t index = 0; index < max_sinks; ++index)
        {
            sinks_[index] = nullptr;
        }
        sink_count_ = 0U;
    }

    [[nodiscard]] bool attach(const ConsoleSink& sink)
    {
        if (!sink.valid())
        {
            return false;
        }

        for (size_t index = 0; index < sink_count_; ++index)
        {
            if (sinks_[index] == &sink)
            {
                return true;
            }
        }

        if (sink_count_ >= max_sinks)
        {
            return false;
        }

        sinks_[sink_count_] = &sink;
        ++sink_count_;
        return true;
    }

    void put(Route route, char ch) const
    {
        for (size_t index = 0; index < sink_count_; ++index)
        {
            const ConsoleSink& sink = *sinks_[index];

            if ((route == Route::emergency) &&
                !sink.supports(SinkCapability::panic_safe))
            {
                continue;
            }

            sink.put(ch);
        }
    }

    void flush(Route route) const
    {
        for (size_t index = 0; index < sink_count_; ++index)
        {
            const ConsoleSink& sink = *sinks_[index];

            if ((route == Route::emergency) &&
                !sink.supports(SinkCapability::panic_safe))
            {
                continue;
            }

            sink.flush();
        }
    }

private:
    const ConsoleSink* sinks_[max_sinks];
    size_t sink_count_;
};

struct MemorySinkState {
    char buffer[memory_sink_size];
    size_t write_position;
    bool wrapped;
};

constinit ConsoleRouter router{};
constinit MemorySinkState memory_state{};
constinit bool initialized = false;

void memory_put_char(void* context, char ch)
{
    auto& state = *static_cast<MemorySinkState*>(context);

    state.buffer[state.write_position] = ch;
    ++state.write_position;

    if (state.write_position == memory_sink_size)
    {
        state.write_position = 0U;
        state.wrapped = true;
    }
}

constinit ConsoleSink memory_sink{
    &memory_state,
    memory_put_char,
    nullptr,
    SinkCapability::early_safe |
        SinkCapability::panic_safe};

void write_unsigned(
    Route route,
    unsigned long long value,
    unsigned int base)
{
    static constexpr char digits[] = "0123456789abcdef";
    char scratch[32];
    size_t count = 0U;

    do
    {
        const unsigned int digit =
            static_cast<unsigned int>(value % base);
        scratch[count] = digits[digit];
        ++count;
        value /= base;
    } while (value != 0U);

    while (count != 0U)
    {
        --count;
        put(route, scratch[count]);
    }
}

void write_signed(Route route, long long value)
{
    if (value < 0)
    {
        put(route, '-');
        const unsigned long long magnitude =
            0ULL - static_cast<unsigned long long>(value);
        write_unsigned(route, magnitude, 10U);
        return;
    }

    write_unsigned(
        route,
        static_cast<unsigned long long>(value),
        10U);
}

void write_hex(Route route, HexValue value)
{
    static constexpr char digits[] = "0123456789abcdef";
    constexpr size_t max_hex_digits = sizeof(uint64_t) * 2U;

    size_t requested_digits = value.digits;
    if (requested_digits > max_hex_digits)
    {
        requested_digits = max_hex_digits;
    }

    if (requested_digits == 0U)
    {
        requested_digits = 1U;
        uint64_t remaining = value.value;
        while ((remaining >>= 4U) != 0U)
        {
            ++requested_digits;
        }
    }

    write(route, "0x");

    for (size_t digit_index = requested_digits;
         digit_index != 0U;
         --digit_index)
    {
        const size_t shift = (digit_index - 1U) * 4U;
        const uint64_t nibble = (value.value >> shift) & 0xFU;
        put(route, digits[nibble]);
    }
}

} // namespace

void initialize()
{
    if (initialized)
    {
        return;
    }

    router.reset();
    (void)router.attach(memory_sink);
    initialized = true;
}

bool attach_sink(const ConsoleSink& sink)
{
    if (!initialized)
    {
        initialize();
    }

    return router.attach(sink);
}

void put(Route route, char ch)
{
    if (!initialized)
    {
        return;
    }

    router.put(route, ch);
}

void write(Route route, const char* text)
{
    if (text == nullptr)
    {
        text = "(null)";
    }

    while (*text != '\0')
    {
        put(route, *text);
        ++text;
    }
}

void flush(Route route)
{
    if (!initialized)
    {
        return;
    }

    router.flush(route);
}

const char* memory_buffer()
{
    return memory_state.buffer;
}

size_t memory_capacity()
{
    return memory_sink_size;
}

size_t memory_write_position()
{
    return memory_state.write_position;
}

bool memory_wrapped()
{
    return memory_state.wrapped;
}

ConsoleStream& ConsoleStream::operator<<(const char* text)
{
    write(route_, text);
    return *this;
}

ConsoleStream& ConsoleStream::operator<<(char ch)
{
    put(route_, ch);
    return *this;
}

ConsoleStream& ConsoleStream::operator<<(bool value)
{
    write(route_, value ? "true" : "false");
    return *this;
}

ConsoleStream& ConsoleStream::operator<<(unsigned int value)
{
    write_unsigned(route_, value, 10U);
    return *this;
}

ConsoleStream& ConsoleStream::operator<<(int value)
{
    write_signed(route_, value);
    return *this;
}

ConsoleStream& ConsoleStream::operator<<(unsigned long value)
{
    write_unsigned(route_, value, 10U);
    return *this;
}

ConsoleStream& ConsoleStream::operator<<(long value)
{
    write_signed(route_, value);
    return *this;
}

ConsoleStream& ConsoleStream::operator<<(unsigned long long value)
{
    write_unsigned(route_, value, 10U);
    return *this;
}

ConsoleStream& ConsoleStream::operator<<(long long value)
{
    write_signed(route_, value);
    return *this;
}

ConsoleStream& ConsoleStream::operator<<(const void* pointer)
{
    write_hex(
        route_,
        hex(reinterpret_cast<uintptr_t>(pointer)));
    return *this;
}

ConsoleStream& ConsoleStream::operator<<(HexValue value)
{
    write_hex(route_, value);
    return *this;
}

} // namespace jixia::microkernel::console
