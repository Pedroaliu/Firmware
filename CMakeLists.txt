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

option(
    JIXIA_M00_06_02_PROBE
    "Boot the M00-06.02 one-way M-to-S supervisor transition probe"
    OFF
)

option(
    JIXIA_M00_06_03_PROBE
    "Boot the M00-06.03 supervisor ECALL round-trip probe"
    OFF
)

option(
    JIXIA_M00_06_04_PROBE
    "Boot the M00-06.04 hostile lower-privilege stack boundary probe"
    OFF
)

option(
    JIXIA_M00_07_03_PROBE
    "Boot the M00-07.03 pre-DDR pflash-backed Sv39 paging probe"
    OFF
)

option(
    JIXIA_M00_07_04_PROBE
    "Boot the M00-07.04 fake DDR and contained-to-mainstore transition probe"
    OFF
)

option(
    JIXIA_M00_08_01_PROBE
    "Boot the M00-08.01 Hostboot-shaped task lifecycle probe"
    OFF
)

set(JIXIA_ARCH_FLAGS
    -march=rv64imac_zicsr_zifencei
    -mabi=lp64
    -mcmodel=medany
)

set(JIXIA_LINKER_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/linker/qemu_virt.ld
)

set(JIXIA_STAGE0_LINKER_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/linker/qemu_virt_stage0.ld
)

set(JIXIA_PAGEABLE_PROBE_LINKER_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/linker/qemu_virt_pageable_probe.ld
)

# CMAKE_OBJCOPY/CMAKE_OBJDUMP/CMAKE_READELF are FILEPATH cache variables.
# Passing a bare command name such as "riscv64-unknown-elf-objcopy" on the
# CMake command line may therefore be normalized into a non-existent path
# relative to the source tree. That happened on the first GitHub Actions run:
# the compiler linked jixia.elf successfully, but the POST_BUILD objcopy step
# tried to execute <repo>/riscv64-unknown-elf-objcopy instead of /usr/bin/....
#
# Resolve the tools explicitly. Existing absolute tool paths are preserved;
# otherwise search PATH, which keeps local development and clean CI runners
# using the same contract.
function(jixia_resolve_binutil out_var cmake_candidate fallback_name)
    if(cmake_candidate AND
       IS_ABSOLUTE "${cmake_candidate}" AND
       EXISTS "${cmake_candidate}")
        set(${out_var} "${cmake_candidate}" PARENT_SCOPE)
        return()
    endif()

    if(cmake_candidate)
        get_filename_component(candidate_name "${cmake_candidate}" NAME)
    else()
        set(candidate_name "${fallback_name}")
    endif()

    set(resolved_var "${out_var}_RESOLVED")

    find_program(
        ${resolved_var}
        NAMES
            "${candidate_name}"
            "${fallback_name}"
        REQUIRED
    )

    set(${out_var} "${${resolved_var}}" PARENT_SCOPE)
endfunction()

jixia_resolve_binutil(
    JIXIA_OBJCOPY
    "${CMAKE_OBJCOPY}"
    riscv64-unknown-elf-objcopy
)

jixia_resolve_binutil(
    JIXIA_OBJDUMP
    "${CMAKE_OBJDUMP}"
    riscv64-unknown-elf-objdump
)

jixia_resolve_binutil(
    JIXIA_READELF
    "${CMAKE_READELF}"
    riscv64-unknown-elf-readelf
)

add_executable(
    jixia.elf

    # Keep the architectural entry source first for readability. The linker
    # script independently pins .text.boot at 0x80000000, so correctness does
    # not depend on this source-list ordering.
    microkernel/arch/riscv/start.S
    microkernel/arch/riscv/trap.S
    microkernel/arch/riscv/trap_frame_test.S
    microkernel/arch/riscv/privilege_transition_test.S
    microkernel/arch/riscv/privilege_boundary_test.S
    microkernel/arch/riscv/pre_ddr_paging_test.S
    microkernel/arch/riscv/task_dispatch.S
    microkernel/arch/riscv/user_task.S
    microkernel/arch/riscv/instruction_decode.cpp
    microkernel/arch/riscv/sv39.cpp

    lib/format.cpp
    lib/memory.cpp
    microkernel/console/kernel_console.cpp
    microkernel/console/printk.cpp
    microkernel/core/firmware_main.cpp
    microkernel/core/kernel.cpp
    microkernel/core/trap.cpp
    microkernel/core/task_context.cpp
    microkernel/core/task_manager.cpp
    microkernel/core/task_syscall.cpp
    microkernel/core/scheduler.cpp
    microkernel/core/heap_manager.cpp
    microkernel/core/vmm_manager.cpp
    microkernel/core/time_manager.cpp
    microkernel/core/cpu_manager.cpp
    microkernel/core/privilege_boundary_test.cpp
    microkernel/core/pre_ddr_paging_test.cpp
    microkernel/core/mainstore_transition_test.cpp
    microkernel/core/timer.cpp
    microkernel/core/smp_timer_test.cpp
    microkernel/core/kernel_print_test.cpp
    microkernel/core/recoverable_trap_test.cpp
    microkernel/core/timer_interrupt_test.cpp
    microkernel/core/trap_frame_test.cpp
    microkernel/firmware_store/ffs.cpp
    microkernel/memory/memory_lifecycle.cpp
    microkernel/memory/page_manager.cpp
    microkernel/memory/flash_provider.cpp
    platform/qemu_virt/contained_memory.cpp
    platform/qemu_virt/ddr.cpp
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

if(JIXIA_M00_06_02_PROBE)
    target_compile_definitions(
        jixia.elf
        PRIVATE
            JIXIA_M00_06_02_PROBE=1
    )
endif()

if(JIXIA_M00_06_03_PROBE)
    target_compile_definitions(
        jixia.elf
        PRIVATE
            JIXIA_M00_06_03_PROBE=1
    )
endif()

if(JIXIA_M00_06_04_PROBE)
    target_compile_definitions(
        jixia.elf
        PRIVATE
            JIXIA_M00_06_04_PROBE=1
    )
endif()

if(JIXIA_M00_07_03_PROBE)
    target_compile_definitions(
        jixia.elf
        PRIVATE
            JIXIA_M00_07_03_PROBE=1
    )
endif()

if(JIXIA_M00_07_04_PROBE)
    target_compile_definitions(
        jixia.elf
        PRIVATE
            JIXIA_M00_07_04_PROBE=1
    )
endif()

if(JIXIA_M00_08_01_PROBE)
    target_compile_definitions(
        jixia.elf
        PRIVATE
            JIXIA_M00_08_01_PROBE=1
    )
endif()

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
        ${JIXIA_OBJCOPY}
        -O binary
        $<TARGET_FILE:jixia.elf>
        ${CMAKE_CURRENT_BINARY_DIR}/jixia.bin

    COMMAND
        ${JIXIA_OBJDUMP}
        -d
        -S
        $<TARGET_FILE:jixia.elf>
        >
        ${CMAKE_CURRENT_BINARY_DIR}/jixia.dis

    COMMAND
        ${JIXIA_READELF}
        -a
        $<TARGET_FILE:jixia.elf>
        >
        ${CMAKE_CURRENT_BINARY_DIR}/jixia.readelf

    COMMENT "Generating Jixia firmware image and inspection files"
)

# M00-07.01: a tiny stackless XIP Stage0 linked at QEMU virt pflash0 base.
add_executable(
    jixia_stage0.elf
    boot/qemu_virt/stage0.S
)

target_include_directories(
    jixia_stage0.elf
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(
    jixia_stage0.elf
    PRIVATE
        ${JIXIA_ARCH_FLAGS}
)

target_link_options(
    jixia_stage0.elf
    PRIVATE
        ${JIXIA_ARCH_FLAGS}
        -nostdlib
        -nostartfiles
        -static
        -Wl,--gc-sections
        -Wl,--build-id=none
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/jixia_stage0.map
        -T${JIXIA_STAGE0_LINKER_SCRIPT}
)

set_property(
    TARGET jixia_stage0.elf
    APPEND PROPERTY LINK_DEPENDS ${JIXIA_STAGE0_LINKER_SCRIPT}
)

add_custom_command(
    TARGET jixia_stage0.elf
    POST_BUILD

    COMMAND
        ${JIXIA_OBJCOPY}
        -O binary
        $<TARGET_FILE:jixia_stage0.elf>
        ${CMAKE_CURRENT_BINARY_DIR}/jixia_stage0.bin

    COMMAND
        ${JIXIA_OBJDUMP}
        -d
        -S
        $<TARGET_FILE:jixia_stage0.elf>
        >
        ${CMAKE_CURRENT_BINARY_DIR}/jixia_stage0.dis

    COMMAND
        ${JIXIA_READELF}
        -a
        $<TARGET_FILE:jixia_stage0.elf>
        >
        ${CMAKE_CURRENT_BINARY_DIR}/jixia_stage0.readelf

    COMMENT "Generating Jixia pflash Stage0 image and inspection files"
)

# M00-07.03: one pflash-resident page linked for a deliberately unmapped S VA.
add_executable(
    jixia_pageable_probe.elf
    boot/qemu_virt/pageable_probe.S
)

target_include_directories(
    jixia_pageable_probe.elf
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(
    jixia_pageable_probe.elf
    PRIVATE
        ${JIXIA_ARCH_FLAGS}
)

target_link_options(
    jixia_pageable_probe.elf
    PRIVATE
        ${JIXIA_ARCH_FLAGS}
        -nostdlib
        -nostartfiles
        -static
        -Wl,--gc-sections
        -Wl,--build-id=none
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/jixia_pageable_probe.map
        -T${JIXIA_PAGEABLE_PROBE_LINKER_SCRIPT}
)

set_property(
    TARGET jixia_pageable_probe.elf
    APPEND PROPERTY LINK_DEPENDS ${JIXIA_PAGEABLE_PROBE_LINKER_SCRIPT}
)

add_custom_command(
    TARGET jixia_pageable_probe.elf
    POST_BUILD

    COMMAND
        ${JIXIA_OBJCOPY}
        -O binary
        $<TARGET_FILE:jixia_pageable_probe.elf>
        ${CMAKE_CURRENT_BINARY_DIR}/jixia_pageable_probe.bin

    COMMAND
        ${JIXIA_OBJDUMP}
        -d
        -S
        $<TARGET_FILE:jixia_pageable_probe.elf>
        >
        ${CMAKE_CURRENT_BINARY_DIR}/jixia_pageable_probe.dis

    COMMENT "Generating M00-07.03 pageable Extended-image probe"
)
