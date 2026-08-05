#include <archfw/console.h>
#include <platform/qemu_virt.h>

#define UART_RBR_THR_DLL 0u
#define UART_IER_DLM     1u
#define UART_FCR_IIR     2u
#define UART_LCR         3u
#define UART_MCR         4u
#define UART_LSR         5u

#define UART_LCR_DLAB    (1u << 7)
#define UART_LCR_8N1     0x03u
#define UART_FCR_ENABLE  0x01u
#define UART_FCR_CLEAR   0x06u
#define UART_MCR_DTR_RTS 0x03u
#define UART_LSR_THRE    (1u << 5)

static volatile uint8_t *const uart =
    (volatile uint8_t *)(ARCHFW_UART0_BASE);

static inline void uart_write(unsigned int reg, uint8_t value)
{
    uart[reg] = value;
}

static inline uint8_t uart_read(unsigned int reg)
{
    return uart[reg];
}

void archfw_console_init(void)
{
    /*
     * QEMU's ns16550 is normally usable immediately, but programming it
     * explicitly makes Boot0 independent of inherited monitor state.
     * 3.6864 MHz / (16 * 2) = 115200 baud.
     */
    uart_write(UART_IER_DLM, 0u);
    uart_write(UART_LCR, UART_LCR_DLAB);
    uart_write(UART_RBR_THR_DLL, 2u);
    uart_write(UART_IER_DLM, 0u);
    uart_write(UART_LCR, UART_LCR_8N1);
    uart_write(UART_FCR_IIR, UART_FCR_ENABLE | UART_FCR_CLEAR);
    uart_write(UART_MCR, UART_MCR_DTR_RTS);
}

void archfw_console_putc(char ch)
{
    if (ch == '\n') {
        archfw_console_putc('\r');
    }

    while ((uart_read(UART_LSR) & UART_LSR_THRE) == 0u) {
        __asm__ volatile("nop");
    }
    uart_write(UART_RBR_THR_DLL, (uint8_t)ch);
}

void archfw_console_puts(const char *text)
{
    if (text == NULL) {
        text = "(null)";
    }

    while (*text != '\0') {
        archfw_console_putc(*text++);
    }
}

void archfw_console_puthex(archfw_word_t value)
{
    static const char digits[] = "0123456789abcdef";
    unsigned int shift = (unsigned int)(sizeof(value) * 8u);

    archfw_console_puts("0x");
    while (shift != 0u) {
        shift -= 4u;
        archfw_console_putc(digits[(value >> shift) & 0x0fu]);
    }
}
