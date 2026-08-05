ARCHFW_ARCH     ?= riscv64
ARCHFW_PLATFORM ?= qemu-virt
BUILD_DIR       := build/$(ARCHFW_PLATFORM)

TOOLCHAIN       ?= llvm
CROSS_COMPILE   ?= riscv64-unknown-elf-
QEMU            ?= qemu-system-riscv64

ifeq ($(TOOLCHAIN),gnu)
CC              := $(CROSS_COMPILE)gcc
OBJCOPY         := $(CROSS_COMPILE)objcopy
TARGET_FLAGS    :=
else ifeq ($(TOOLCHAIN),llvm)
CC              := clang
OBJCOPY         := llvm-objcopy
TARGET_FLAGS    := --target=riscv64-unknown-elf
else
$(error TOOLCHAIN must be either llvm or gnu)
endif

TARGET          := $(BUILD_DIR)/archfw
ELF             := $(TARGET).elf
BIN             := $(TARGET).bin
MAP             := $(TARGET).map

CPPFLAGS        := -Iinclude -DARCHFW_PLATFORM_QEMU_VIRT=1
COMMON_FLAGS    := $(TARGET_FLAGS) \
                   -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany \
                   -ffreestanding -fno-builtin -fno-common \
                   -fno-stack-protector -fno-pic -fno-pie \
                   -fdata-sections -ffunction-sections \
                   -msmall-data-limit=0 \
                   -Wall -Wextra -Werror -O2 -g
CFLAGS          := $(COMMON_FLAGS)
ASFLAGS         := $(COMMON_FLAGS)
LDFLAGS         := $(TARGET_FLAGS) -nostdlib \
                   -Wl,-T,linker/qemu-virt.ld \
                   -Wl,--gc-sections \
                   -Wl,-Map,$(MAP) \
                   -Wl,--build-id=none

C_SRCS          := kernel/main.c \
                   kernel/trap.c \
                   drivers/uart/ns16550.c
S_SRCS          := arch/riscv64/start.S \
                   arch/riscv64/trap.S
OBJS            := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
                   $(patsubst %.S,$(BUILD_DIR)/%.o,$(S_SRCS))

.PHONY: all clean run debug print-config

all: $(ELF) $(BIN)

$(ELF): $(OBJS) linker/qemu-virt.ld
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

run: all
	$(QEMU) -machine virt -m 128M -smp 1 -nographic -bios $(BIN)

debug: all
	$(QEMU) -machine virt -m 128M -smp 1 -nographic -bios $(BIN) -S -s

print-config:
	@echo "ARCHFW_ARCH=$(ARCHFW_ARCH)"
	@echo "ARCHFW_PLATFORM=$(ARCHFW_PLATFORM)"
	@echo "TOOLCHAIN=$(TOOLCHAIN)"
	@echo "CC=$(CC)"
	@echo "OBJCOPY=$(OBJCOPY)"
	@echo "QEMU=$(QEMU)"

clean:
	rm -rf build
