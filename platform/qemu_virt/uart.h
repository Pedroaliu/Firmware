#pragma once

#include <stdint.h>

void uart_putc(char ch);
void uart_puts(const char *str);
void uart_put_hex_uintptr(uintptr_t value);