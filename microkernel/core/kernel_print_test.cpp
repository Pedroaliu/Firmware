#include <stddef.h>
#include <stdint.h>

#include "microkernel/console/kernel_console.h"
#include "microkernel/console/printk.h"

namespace jixia::microkernel::kernel_print_test {
namespace {

[[nodiscard]]
bool buffer_matches(
    size_t offset,
    const char* expected)
{
    const char* const log = kernel_console::buffer();
    size_t index = 0U;

    while (expected[index] != '\0')
    {
        if ((offset + index) >= kernel_console::size())
        {
            return false;
        }

        if (log[offset + index] != expected[index])
        {
            return false;
        }

        ++index;
    }

    return true;
}

[[noreturn]]
void fail(const char* reason)
{
    printk("KERNEL_PRINT_TEST: FAIL (%s)\n", reason);
    for (;;)
    {
        __asm__ volatile("wfi");
    }
}

} // namespace

void run()
{
    printk("\n[Jixia][Test][KernelPrint]\n");

    const size_t probe_offset = kernel_console::size();

    printk(
        "probe      : s=%s d=%d u=%u x=%08x p=%p %%\n",
        "ok",
        -42,
        42U,
        0x1A2BU,
        reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234U)));

    static constexpr char expected[] =
        "probe      : s=ok d=-42 u=42 x=00001a2b "
        "p=0x0000000000001234 %\n";

    if (!buffer_matches(probe_offset, expected))
    {
        fail("buffer mismatch");
    }

    if (kernel_console::truncated())
    {
        fail("unexpected truncation");
    }

    printk(
        "buffer     : append-only kernel log retained exact probe\n"
        "capacity   : %zu bytes\n"
        "KERNEL_PRINT_TEST: PASS\n",
        kernel_console::buffer_capacity);
}

} // namespace jixia::microkernel::kernel_print_test

extern "C" void jixia_kernel_print_test()
{
    jixia::microkernel::kernel_print_test::run();
}
