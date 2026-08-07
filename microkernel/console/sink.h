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

[[nodiscard]] constexpr SinkCapability operator|(
    SinkCapability lhs,
    SinkCapability rhs)
{
    return static_cast<SinkCapability>(
        static_cast<uint32_t>(lhs) |
        static_cast<uint32_t>(rhs));
}

[[nodiscard]] constexpr bool has_capability(
    SinkCapability capabilities,
    SinkCapability requested)
{
    return (static_cast<uint32_t>(capabilities) &
            static_cast<uint32_t>(requested)) != 0U;
}

class ConsoleSink final
{
public:
    using PutCharFn = void (*)(void* context, char ch);
    using FlushFn = void (*)(void* context);

    constexpr ConsoleSink(
        void* context,
        PutCharFn put_char,
        FlushFn flush,
        SinkCapability capabilities)
        : context_(context),
          put_char_(put_char),
          flush_(flush),
          capabilities_(capabilities)
    {
    }

    [[nodiscard]] constexpr bool valid() const
    {
        return put_char_ != nullptr;
    }

    [[nodiscard]] constexpr bool supports(
        SinkCapability capability) const
    {
        return has_capability(capabilities_, capability);
    }

    [[nodiscard]] constexpr SinkCapability capabilities() const
    {
        return capabilities_;
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
    PutCharFn put_char_;
    FlushFn flush_;
    SinkCapability capabilities_;
};

} // namespace jixia::microkernel::console
