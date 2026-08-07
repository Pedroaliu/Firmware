#include "uart.h"

namespace jixia::microkernel::trap_test {

void run_recoverable_trap_test()
{
    uart_puts("\n[Jixia][Test][RecoverableTrap]\n");

    /*
     * Use explicit encodings so the test controls instruction length exactly.
     * EBREAK must resume at the instruction immediately following these bytes.
     */
    __asm__ volatile(".word 0x00100073" ::: "memory");
    uart_puts("standard   : resumed after 32-bit EBREAK\n");

    __asm__ volatile(".hword 0x9002" ::: "memory");
    uart_puts("compressed : resumed after 16-bit C.EBREAK\n");

    uart_puts("RECOVERABLE_TRAP_TEST: PASS\n");
}

} // namespace jixia::microkernel::trap_test

extern "C" void jixia_recoverable_trap_test()
{
    jixia::microkernel::trap_test::run_recoverable_trap_test();
}
