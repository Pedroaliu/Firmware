#include "microkernel/arch/riscv/pre_ddr_paging_test_values.h"

/*
 * M00-07.03 Extended-image payload.
 *
 * This page is deliberately not copied by Stage0. It is linked for the virtual
 * address that the S-mode probe calls. The first instruction fetch therefore
 * must fault until the resident M-mode pager supplies a backing page from
 * pflash and installs the Sv39 mapping.
 */

.section .text.pageable_probe, "ax", @progbits

.global jixia_m00_07_03_pageable_entry
.type jixia_m00_07_03_pageable_entry, @function

jixia_m00_07_03_pageable_entry:
    li a0, M00_07_03_PAGEABLE_RESULT
    ret

.size jixia_m00_07_03_pageable_entry, . - jixia_m00_07_03_pageable_entry
