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

    .text : ALIGN(4096)
    {
        __text_start = .;

        /*
         * jixia.bin is a raw firmware image. QEMU virt loads it at
         * 0x80000000 and its reset ROM transfers control to that fixed
         * address; the raw image carries no ELF e_entry metadata.
         *
         * Keep the boot section first so _start is always the first
         * instruction in the image, independent of compiler/linker input
         * section ordering as the firmware grows.
         */
        KEEP(*(.text.boot))

        *(.text)
        *(.text.*)

        __text_end = .;
    } > RAM :text

    ASSERT(_start == ORIGIN(RAM),
           "Jixia firmware _start must remain at RAM origin 0x80000000")

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
         * RISC-V can address small data through gp. start.S initializes gp
         * explicitly before entering C++ code.
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

        KEEP(*(.stack.harts))
        KEEP(*(.stack.harts.*))

        __stack_top = .;
    } > RAM

    /*
     * M00-07 bootstrap physical pages. This is allocator-owned contained
     * memory, not part of the resident firmware image. The pool is explicitly
     * page aligned so Sv39 page tables and paged-in firmware can share the
     * same simple PageManager contract.
     */
    .early_memory_pool (NOLOAD) : ALIGN(4096)
    {
        __early_page_pool_start = .;
        . += 64K;
        __early_page_pool_end = .;
    } > RAM

    ASSERT(__early_page_pool_end <= ORIGIN(RAM) + 8M,
           "M00-07 early page pool must stay inside the contained 8 MiB window")

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
