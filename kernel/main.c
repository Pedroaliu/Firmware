#include <archfw/console.h>
#include <archfw/csr.h>
#include <archfw/types.h>
#include <platform/qemu_virt.h>

static void print_field(const char *name, archfw_word_t value)
{
    archfw_console_puts("[archfw] ");
    archfw_console_puts(name);
    archfw_console_puts(": ");
    archfw_console_puthex(value);
    archfw_console_putc('\n');
}

void archfw_kernel_main(archfw_word_t hart_id, archfw_word_t fdt_address)
{
    archfw_console_init();

    archfw_console_puts("\n");
    archfw_console_puts("ArchFW microkernel M00\n");
    archfw_console_puts("======================\n");
    archfw_console_puts("[archfw] phase    : KERNEL_BOOTSTRAP\n");
    archfw_console_puts("[archfw] platform : " ARCHFW_PLATFORM_NAME "\n");
    print_field("hart     ", hart_id);
    print_field("fdt      ", fdt_address);
    print_field("mtvec    ", archfw_csr_read_mtvec());
    print_field("mhartid  ", archfw_csr_read_mhartid());
    archfw_console_puts("[archfw] uart     : ns16550 @ 0x0000000010000000\n");
    archfw_console_puts("[archfw] status   : M00 UART bootstrap complete\n");
    archfw_console_puts("[archfw] idle\n");

    for (;;) {
        __asm__ volatile("wfi");
    }
}
