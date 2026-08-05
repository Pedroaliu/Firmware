#pragma once

#include <archfw/types.h>

static inline archfw_word_t archfw_csr_read_mhartid(void)
{
    archfw_word_t value;
    __asm__ volatile("csrr %0, mhartid" : "=r"(value));
    return value;
}

static inline archfw_word_t archfw_csr_read_mtvec(void)
{
    archfw_word_t value;
    __asm__ volatile("csrr %0, mtvec" : "=r"(value));
    return value;
}
