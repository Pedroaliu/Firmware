#pragma once

/*
 * Fixed RV64 register patterns used by the TrapFrame test.
 *
 * sp, gp, and tp deliberately retain their live firmware values. The test
 * snapshots those values immediately before EBREAK and validates the saved
 * TrapFrame against the snapshots.
 */
#define TRAP_TEST_FIXED_GPRS(X)      \
    X(1,  ra,  0x0101010101010101)  \
    X(5,  t0,  0x0505050505050505)  \
    X(6,  t1,  0x0606060606060606)  \
    X(7,  t2,  0x0707070707070707)  \
    X(8,  s0,  0x0808080808080808)  \
    X(9,  s1,  0x0909090909090909)  \
    X(10, a0,  0x0a0a0a0a0a0a0a0a)  \
    X(11, a1,  0x0b0b0b0b0b0b0b0b)  \
    X(12, a2,  0x0c0c0c0c0c0c0c0c)  \
    X(13, a3,  0x0d0d0d0d0d0d0d0d)  \
    X(14, a4,  0x0e0e0e0e0e0e0e0e)  \
    X(15, a5,  0x0f0f0f0f0f0f0f0f)  \
    X(16, a6,  0x1010101010101010)  \
    X(17, a7,  0x1111111111111111)  \
    X(18, s2,  0x1212121212121212)  \
    X(19, s3,  0x1313131313131313)  \
    X(20, s4,  0x1414141414141414)  \
    X(21, s5,  0x1515151515151515)  \
    X(22, s6,  0x1616161616161616)  \
    X(23, s7,  0x1717171717171717)  \
    X(24, s8,  0x1818181818181818)  \
    X(25, s9,  0x1919191919191919)  \
    X(26, s10, 0x1a1a1a1a1a1a1a1a)  \
    X(27, s11, 0x1b1b1b1b1b1b1b1b)  \
    X(28, t3,  0x1c1c1c1c1c1c1c1c)  \
    X(29, t4,  0x1d1d1d1d1d1d1d1d)  \
    X(30, t5,  0x1e1e1e1e1e1e1e1e)  \
    X(31, t6,  0x1f1f1f1f1f1f1f1f)
