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

set(JIXIA_COMMON_FLAGS
    ${JIXIA_ARCH_FLAGS}

    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-pic
    -fno-pie

    -ffunction-sections
    -fdata-sections

    -Wall
    -Wextra
    -Werror
)

add_executable(
    jixia.elf

    microkernel/arch/riscv/start.S
    microkernel/arch/riscv/trap.S
    microkernel/core/firmware_main.cpp
    microkernel/core/trap.cpp
    platform/qemu_virt/uart.c
)

target_include_directories(
    jixia.elf
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/qemu_virt
)

target_compile_options(
    jixia.elf
    PRIVATE
        ${JIXIA_COMMON_FLAGS}
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

        -T${CMAKE_CURRENT_SOURCE_DIR}/linker/qemu_virt.ld
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
