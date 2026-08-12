#pragma once

/*
 * Shared register markers for the M00-06.03 supervisor ECALL round-trip probe.
 *
 * This header is included by both preprocessed assembly and C++. Keep values
 * numeric and free of language-specific suffixes so both sides consume the
 * exact same test contract.
 */
#define M00_06_03_GP_MARKER 0x3333333333333333
#define M00_06_03_A0_MARKER 0xaaaaaaaaaaaaaaaa
#define M00_06_03_A7_MARKER 0x7777777777777777
#define M00_06_03_ECALL_INSTRUCTION_BYTES 4
