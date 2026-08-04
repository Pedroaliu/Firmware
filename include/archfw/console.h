#pragma once

#include <archfw/types.h>

void archfw_console_init(void);
void archfw_console_putc(char ch);
void archfw_console_puts(const char *text);
void archfw_console_puthex(archfw_word_t value);
