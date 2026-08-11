#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

namespace jixia::format {

class Writer final
{
public:
    using PutChar = void (*)(void* context, char ch);

    constexpr Writer(void* context, PutChar put_char)
        : context_(context), put_char_(put_char)
    {
    }

    [[nodiscard]]
    constexpr bool valid() const
    {
        return put_char_ != nullptr;
    }

    void put(char ch) const
    {
        put_char_(context_, ch);
    }

private:
    void* context_;
    PutChar put_char_;
};

size_t vformat(Writer writer, const char* format, va_list& args);

} // namespace jixia::format
