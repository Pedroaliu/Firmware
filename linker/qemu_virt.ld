OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

PHDRS
{
    text PT_LOAD FLAGS((5));
    data PT_LOAD FLAGS((6));
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : ALIGN(4096) {
        __text_start = .;

        KEEP(*(.text))

        *(.text)
        *(.text.*)

        __text_end = .;
    } > RAM :text

    .rodata : ALIGN(4096)
    {
        __rodata_start = .;

        KEEP(*(.rodata))

        *(.rodata)
        *(.rodata.*)

        __rodata_end = .;
    } > RAM :text

    .data : ALIGN(4096)
    {
        __data_start = .;

        /*
         * RISC-V可通过gp访问small data。
         * 后面的start.S会显式初始化gp。
         */
        PROVIDE(__global_pointer$ = . + 0x800);

        *(.sdata)
        *(.sdata.*)
        *(.data)
        *(.data.*)

        __data_end = .;
    } > RAM :data

     .bss (NOLOAD) : ALIGN(16)
    {
        __bss_start = .;

        *(.sbss)
        *(.sbss.*)
        *(.bss)
        *(.bss.*)
        *(COMMON)

        __bss_end = .;
    } > RAM :data

    .stack (NOLOAD) : ALIGN(16)
    {
        __stack_bottom = .;

        /*
         * M00暂时只有一个Boot Hart。
         */
        . += 16K;

        __stack_top = .;
    } > RAM

    __image_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame.*)
    }
}
