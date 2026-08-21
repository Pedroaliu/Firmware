#include "microkernel/console/printk.h"

#include <stdarg.h>

#include "lib/format.h"
#include "microkernel/console/kernel_console.h"
#include "microkernel/core/spinlock.h"

namespace jixia::microkernel {
namespace {

/*
 * M00-08.03.02: with blocking IPC, two harts legitimately emit kernel markers
 * at the same time (block on one hart, wake on the other). One spinlock per
 * printk call keeps each formatted line atomic in the console buffer and on
 * the UART. This is a leaf lock: the console internals take no other locks,
 * so printk stays callable from any non-nested trap context.
 */
Spinlock g_print_lock;

void kernel_put_char(void*, char ch)
{
    kernel_console::put(ch);
}

} // namespace

int printk(const char* format, ...)
{
    SpinlockGuard guard(g_print_lock);

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
