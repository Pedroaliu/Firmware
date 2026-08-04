#include "uart.h"

#include <stdint.h>

#define QEMU_VIRT_UART0_BASE 0x10000000

enum 
{
    UART_THR = 0x00, // Transmit Holding Register (write)
    UART_LSR = 5, // Line Status Register (read)

    UART_LSR_THRE = 1 << 5, // Transmitter Holding Register Empty   

};

static volatile uint8_t *const uart = (volatile uint8_t *)QEMU_VIRT_UART0_BASE;

void uart_putc(char ch)
{
    while (!(uart[UART_LSR] & UART_LSR_THRE))
        ;
    uart[UART_THR] = ch;
}

void uart_puts(const char *str)
{
    while (*str != '\0')
    {
        if (*str == '\n')
            uart_putc('\r');

        uart_putc(*str);
        str++;
    }
}

void uart_put_hex_uintptr(uintptr_t value)
{
    static const char digits[] = "0123456789abcdef";

    uart_puts("0x");

    for (int shift = (int)(sizeof(uintptr_t) * 8U) - 4;
         shift >= 0;
         shift -= 4)
    {
        const uintptr_t nibble = (value >> shift) & 0xFU;
        uart_putc(digits[nibble]);
    }
}