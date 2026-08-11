#include "microkernel/console/printk.h"

#include <stdarg.h>

#include "lib/format.h"
#include "microkernel/console/kernel_console.h"

namespace jixia::microkernel {
namespace {

void kernel_put_char(void*, char ch)
{
    kernel_console::put(ch);
}

} // namespace

int printk(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    const size_t count =
        jixia::format::vformat(
            jixia::format::Writer{nullptr, kernel_put_char},
            format,
            args);

    va_end(args);
    return static_cast<int>(count);
}

} // namespace jixia::microkernel
