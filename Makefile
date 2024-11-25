# EnviLog - ESP32-S3 Environmental Monitoring System
#
# Variable Assignment Notes:
# ':=' Simple expansion - value evaluated once, use for constants, paths, tool names
# '=' Recursive expansion - value evaluated each use, use for dynamic values

# Directory Structure
STARTUP_DIR := ./startup
HAL_DIR     := ./hal
DRIVERS_DIR := ./drivers
CORE_DIR    := ./core
LIB_DIR     := ./lib/minimal
BUILD_DIR   := ./build

# Toolchain
TOOLCHAIN   := xtensa-esp-elf-
CC          := $(TOOLCHAIN)gcc
OBJCOPY     := $(TOOLCHAIN)objcopy
SIZE        := $(TOOLCHAIN)size
ARCH        := esp32s3

# Base flags
ARCH_FLAGS  := -mlongcalls -mtext-section-literals
C_STD       := -std=gnu11
C_WARN      := -Wall -Wextra
C_DEBUG     := -Og -ggdb

# Include paths
C_INCLUDES  := -I$(HAL_DIR) \
               -I$(HAL_DIR)/esp32s3 \
               -I$(HAL_DIR)/gpio \
               -I$(HAL_DIR)/system \
               -I$(HAL_DIR)/uart \
               -I$(DRIVERS_DIR) \
               -I$(CORE_DIR) \
               -I$(LIB_DIR)

# Compiler and linker flags
CFLAGS      = $(ARCH_FLAGS) $(C_STD) $(C_WARN) $(C_DEBUG) $(C_INCLUDES)
LDSCRIPT    := $(STARTUP_DIR)/linker.ld
LD_MAP      := -Wl,-Map=$(BUILD_DIR)/envilog.map
LDFLAGS     = $(ARCH_FLAGS) -T $(LDSCRIPT) $(LD_MAP)
DEPFLAGS    := -MMD -MP

# Source files - explicit paths
STARTUP_SRC := $(STARTUP_DIR)/startup.c
HAL_SRC     := $(HAL_DIR)/gpio/gpio.c \
               $(HAL_DIR)/system/system.c
CORE_SRC    := $(CORE_DIR)/main.c

# All sources and objects
SRCS        := $(STARTUP_SRC) $(HAL_SRC) $(CORE_SRC)
OBJS        := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SRCS)))

# OS detection for serial port
ifeq ($(OS),Windows_NT)
    SERIAL_PORT := COM3
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        SERIAL_PORT := /dev/ttyUSB0
    endif
    ifeq ($(UNAME_S),Darwin)
        SERIAL_PORT := /dev/cu.usbserial-14110
    endif
endif

# Targets
.PHONY: all clean flash monitor debug debug-srcs

all: $(BUILD_DIR)/envilog.bin size

# Explicit object rules
$(BUILD_DIR)/startup.o: $(STARTUP_DIR)/startup.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $(DEPFLAGS) -o $@ $<

$(BUILD_DIR)/gpio.o: $(HAL_DIR)/gpio/gpio.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $(DEPFLAGS) -o $@ $<

$(BUILD_DIR)/system.o: $(HAL_DIR)/system/system.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $(DEPFLAGS) -o $@ $<

$(BUILD_DIR)/main.o: $(CORE_DIR)/main.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $(DEPFLAGS) -o $@ $<

$(BUILD_DIR)/envilog.elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/envilog.bin: $(BUILD_DIR)/envilog.elf
	$(OBJCOPY) -O binary $< $@

size: $(BUILD_DIR)/envilog.elf
	$(SIZE) $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)/*

flash: $(BUILD_DIR)/envilog.bin
	esptool.py --chip $(ARCH) -p $(SERIAL_PORT) -b 460800 \
		--before default_reset \
		--after hard_reset \
		write_flash --flash_mode dio --flash_freq 80m --flash_size detect \
		0x0 $<

monitor:
	screen $(SERIAL_PORT) 115200

debug:
	@echo "Building for debug..."
	$(MAKE) CFLAGS="$(CFLAGS) -DDEBUG"

debug-srcs:
	@echo "STARTUP_SRC: $(STARTUP_SRC)"
	@echo "HAL_SRC: $(HAL_SRC)"
	@echo "CORE_SRC: $(CORE_SRC)"
	@echo "ALL SRCS: $(SRCS)"
	@echo "OBJS: $(OBJS)"

# Include dependency files
-include $(BUILD_DIR)/*.d
