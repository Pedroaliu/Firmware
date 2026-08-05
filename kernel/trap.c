#include <archfw/console.h>
#include <archfw/types.h>

__attribute__((noreturn))
void archfw_trap_panic(archfw_word_t mcause,
                       archfw_word_t mepc,
                       archfw_word_t mtval)
{
    archfw_console_puts("\n[archfw] fatal trap\n");
    archfw_console_puts("[archfw] mcause : ");
    archfw_console_puthex(mcause);
    archfw_console_putc('\n');
    archfw_console_puts("[archfw] mepc   : ");
    archfw_console_puthex(mepc);
    archfw_console_putc('\n');
    archfw_console_puts("[archfw] mtval  : ");
    archfw_console_puthex(mtval);
    archfw_console_putc('\n');

    for (;;) {
        __asm__ volatile("wfi");
    }
}
