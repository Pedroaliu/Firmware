OUTPUT_ARCH("riscv")
ENTRY(jixia_stage0_start)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x20000000, LENGTH = 0x1000
}

SECTIONS
{
    . = ORIGIN(FLASH);

    .text : ALIGN(4)
    {
        __stage0_start = .;

        KEEP(*(.text.boot0))
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)

        __stage0_end = .;
    } > FLASH

    ASSERT(jixia_stage0_start == ORIGIN(FLASH),
           "Jixia Stage0 must begin at QEMU pflash0 base 0x20000000")

    ASSERT(__stage0_end <= ORIGIN(FLASH) + LENGTH(FLASH),
           "Jixia Stage0 must fit before the 0x1000 flash header slot")

    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame.*)
    }
}
