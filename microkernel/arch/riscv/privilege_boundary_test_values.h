#pragma once

/*
 * Shared values for the M00-06.04 privilege-boundary acceptance probe.
 *
 * The hostile x2/sp value is deliberately outside the QEMU virt RAM/MMIO
 * ranges used by Jixia. Trap entry must preserve it only as a register value;
 * any attempt to construct privileged state through that address should fail
 * before the acceptance marker can be emitted.
 */
#define M00_06_04_HOSTILE_SP_MARKER 0xdeadbeefdeadbeef
#define M00_06_04_GP_MARKER 0x4444444444444444
#define M00_06_04_A0_MARKER 0xbbbbbbbbbbbbbbbb
#define M00_06_04_A7_MARKER 0x8888888888888888
#define M00_06_04_ECALL_INSTRUCTION_BYTES 4
