cmake_minimum_required(VERSION 3.20)

project(
    jixia_firmware
    VERSION 0.0.1
    LANGUAGES C CXX ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JIXIA_ARCH_FLAGS
    -march=rv64imac_zicsr
    -mabi=lp64
    -mcmodel=medany
)

set(JIXIA_LINKER_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/linker/qemu_virt.ld
)

add_executable(
    jixia.elf

    # Keep the architectural entry source first for readability. The linker
    # script independently pins .text.boot at 0x80000000, so correctness does
    # not depend on this source-list ordering.
    microkernel/arch/riscv/start.S
    microkernel/arch/riscv/trap.S
    microkernel/arch/riscv/trap_frame_test.S
    microkernel/arch/riscv/instruction_decode.cpp

    lib/format.cpp
    microkernel/console/kernel_console.cpp
    microkernel/console/printk.cpp
    microkernel/core/firmware_main.cpp
    microkernel/core/trap.cpp
    microkernel/core/timer.cpp
    microkernel/core/smp_timer_test.cpp
    microkernel/core/kernel_print_test.cpp
    microkernel/core/recoverable_trap_test.cpp
    microkernel/core/timer_interrupt_test.cpp
    microkernel/core/trap_frame_test.cpp
    platform/qemu_virt/timer.cpp
    platform/qemu_virt/uart.c
        microkernel/arch/riscv/hart_layout.h
        microkernel/core/hart.h
        microkernel/core/hart.cpp
        microkernel/core/smp_timer_test.h
        microkernel/arch/riscv/hart_layout_defs.h
        lib/fdt.h
        lib/fdt.cpp
)

target_include_directories(
    jixia.elf
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/qemu_virt
)

target_compile_options(
    jixia.elf
    PRIVATE
        ${JIXIA_ARCH_FLAGS}

        $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>
        $<$<COMPILE_LANGUAGE:C,CXX>:-fno-builtin>
        $<$<COMPILE_LANGUAGE:C,CXX>:-fno-stack-protector>
        $<$<COMPILE_LANGUAGE:C,CXX>:-fno-pic>
        $<$<COMPILE_LANGUAGE:C,CXX>:-fno-pie>
        $<$<COMPILE_LANGUAGE:C,CXX>:-ffunction-sections>
        $<$<COMPILE_LANGUAGE:C,CXX>:-fdata-sections>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Werror>

        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
)

target_link_options(
    jixia.elf
    PRIVATE
        ${JIXIA_ARCH_FLAGS}

        -nostdlib
        -nostartfiles
        -static

        -Wl,--gc-sections
        -Wl,--build-id=none
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/jixia.map

        -T${JIXIA_LINKER_SCRIPT}
)

# A linker script passed only with -T is not a reliable Ninja dependency on
# every CMake/toolchain combination. Make it explicit so changing the firmware
# memory layout always forces a relink.
set_property(
    TARGET jixia.elf
    APPEND PROPERTY LINK_DEPENDS ${JIXIA_LINKER_SCRIPT}
)

add_custom_command(
    TARGET jixia.elf
    POST_BUILD

    COMMAND
        ${CMAKE_OBJCOPY}
        -O binary
        $<TARGET_FILE:jixia.elf>
        ${CMAKE_CURRENT_BINARY_DIR}/jixia.bin

    COMMAND
        ${CMAKE_OBJDUMP}
        -d
        -S
        $<TARGET_FILE:jixia.elf>
        >
        ${CMAKE_CURRENT_BINARY_DIR}/jixia.dis

    COMMAND
        ${CMAKE_READELF}
        -a
        $<TARGET_FILE:jixia.elf>
        >
        ${CMAKE_CURRENT_BINARY_DIR}/jixia.readelf

    COMMENT "Generating Jixia firmware image and inspection files"
)
