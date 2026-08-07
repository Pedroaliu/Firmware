#pragma once

#include <stdint.h>

namespace jixia::microkernel::console {

enum class SinkCapability : uint32_t {
    none = 0U,
    early_safe = 1U << 0U,
    panic_safe = 1U << 1U,
    blocking = 1U << 2U,
    runtime_only = 1U << 3U,
};

[[nodiscard]]
constexpr SinkCapability operator|(
    SinkCapability lhs,
    SinkCapability rhs)
{
    return static_cast<SinkCapability>(
        static_cast<uint32_t>(lhs) |
        static_cast<uint32_t>(rhs));
}

class ConsoleSink final
{
public:
    using PutChar = void (*)(void* context, char ch);
    using Flush = void (*)(void* context);

    constexpr ConsoleSink(
        void* context,
        PutChar put_char,
        Flush flush,
        SinkCapability capabilities)
        : context_(context),
          put_char_(put_char),
          flush_(flush),
          capabilities_(capabilities)
    {
    }

    [[nodiscard]]
    constexpr bool valid() const
    {
        return put_char_ != nullptr;
    }

    [[nodiscard]]
    constexpr bool supports(SinkCapability capability) const
    {
        return
            (static_cast<uint32_t>(capabilities_) &
             static_cast<uint32_t>(capability)) != 0U;
    }

    void put(char ch) const
    {
        put_char_(context_, ch);
    }

    void flush() const
    {
        if (flush_ != nullptr)
        {
            flush_(context_);
        }
    }

private:
    void* context_;
    PutChar put_char_;
    Flush flush_;
    SinkCapability capabilities_;
};

} // namespace jixia::microkernel::console
