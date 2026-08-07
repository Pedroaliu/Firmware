#include <stddef.h>

#include "microkernel/console/console.h"

namespace jixia::microkernel::console_test {

void run()
{
    const size_t before = console::memory_write_position();

    console::out << "\n[Jixia][Test][Console]\n";

    const size_t after = console::memory_write_position();
    if ((after == before) && !console::memory_wrapped())
    {
        console::emergency << "CONSOLE_TEST: FAIL\n";
        for (;;)
        {
            __asm__ volatile("wfi");
        }
    }

    console::out
        << "memory     : ring sink observed output\n"
        << "capacity   : " << console::memory_capacity() << " bytes\n"
        << "CONSOLE_TEST: PASS\n";
}

} // namespace jixia::microkernel::console_test

extern "C" void jixia_console_test()
{
    jixia::microkernel::console_test::run();
}
