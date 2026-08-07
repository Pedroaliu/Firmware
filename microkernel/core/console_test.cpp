#include <stddef.h>

#include "microkernel/console/console.h"

namespace jixia::microkernel::console_test {
namespace {

[[noreturn]] void fail()
{
    console::emergency << "CONSOLE_TEST: FAIL\n";
    for (;;)
    {
        __asm__ volatile("wfi");
    }
}

} // namespace

void run()
{
    static constexpr char probe[] = "console-memory-probe";

    console::out << "\n[Jixia][Test][Console]\n";

    const size_t before = console::memory_write_position();
    const size_t capacity = console::memory_capacity();

    console::out << probe << '\n';

    const char* const buffer = console::memory_buffer();
    size_t probe_length = 0U;
    while (probe[probe_length] != '\0')
    {
        const size_t position = (before + probe_length) % capacity;
        if (buffer[position] != probe[probe_length])
        {
            fail();
        }
        ++probe_length;
    }

    const size_t expected_after =
        (before + probe_length + 1U) % capacity;
    if (console::memory_write_position() != expected_after)
    {
        fail();
    }

    console::out
        << "memory     : ring sink retained exact probe\n"
        << "capacity   : " << capacity << " bytes\n"
        << "CONSOLE_TEST: PASS\n";
}

} // namespace jixia::microkernel::console_test

extern "C" void jixia_console_test()
{
    jixia::microkernel::console_test::run();
}
