cmake_minimum_required(VERSION 3.20)

project(
    archfw
    VERSION 0.0.1
    LANGUAGES C ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(ARCHFW_ARCH_FLAGS
    -march=rv64imac_zicsr
    -mabi=lp64
    -mcmodel=medany
)

set(ARCHFW_COMMON_FLAGS
    ${ARCHFW_ARCH_FLAGS}

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
    archfw.elf

    kernel/arch/riscv/start.S
    kernel/core/kernel_main.c
    platform/qemu_virt/uart.c
    kernel/arch/riscv/trap.S
    kernel/core/trap.c
)

target_include_directories(
    archfw.elf
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/qemu_virt
)

target_compile_options(
    archfw.elf
    PRIVATE
        ${ARCHFW_COMMON_FLAGS}
)

target_link_options(
    archfw.elf
    PRIVATE
        ${ARCHFW_ARCH_FLAGS}

        -nostdlib
        -nostartfiles
        -static

        -Wl,--gc-sections
        -Wl,--build-id=none
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/archfw.map

        -T${CMAKE_CURRENT_SOURCE_DIR}/linker/qemu_virt.ld
)

add_custom_command(
    TARGET archfw.elf
    POST_BUILD

    COMMAND
        ${CMAKE_OBJCOPY}
        -O binary
        $<TARGET_FILE:archfw.elf>
        ${CMAKE_CURRENT_BINARY_DIR}/archfw.bin

    COMMAND
        ${CMAKE_OBJDUMP}
        -d
        -S
        $<TARGET_FILE:archfw.elf>
        >
        ${CMAKE_CURRENT_BINARY_DIR}/archfw.dis

    COMMAND
        ${CMAKE_READELF}
        -a
        $<TARGET_FILE:archfw.elf>
        >
        ${CMAKE_CURRENT_BINARY_DIR}/archfw.readelf

    COMMENT "Generating raw firmware image and inspection files"
)