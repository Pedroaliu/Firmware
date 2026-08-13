OUTPUT_ARCH("riscv")
ENTRY(jixia_m00_07_03_pageable_entry)

MEMORY
{
    PAGE (rx) : ORIGIN = 0x40000000, LENGTH = 0x1000
}

SECTIONS
{
    . = ORIGIN(PAGE);

    .text : ALIGN(4)
    {
        __pageable_probe_start = .;
        KEEP(*(.text.pageable_probe))
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        __pageable_probe_end = .;
    } > PAGE

    ASSERT(jixia_m00_07_03_pageable_entry == ORIGIN(PAGE),
           "M00-07.03 pageable probe must begin at 0x40000000")

    ASSERT(__pageable_probe_end <= ORIGIN(PAGE) + LENGTH(PAGE),
           "M00-07.03 pageable probe must fit in one 4 KiB page")

    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame.*)
    }
}
