# HIK BIOS Bootloader Makefile

# Compiler and tools
CC = gcc
AS = nasm
LD = ld
OBJCOPY = objcopy

# Compiler flags
CFLAGS = -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
         -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -O2 \
         -I../mbr -I../stage2 -I../hal -I../fs -I../security -I../util \
         -I../../uefi/efi

# Assembler flags
ASFLAGS = -f bin

# Linker flags
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# Directories
MBR_DIR = ../mbr
STAGE2_DIR = ../stage2
HAL_DIR = ../hal
FS_DIR = ../fs
SECURITY_DIR = ../security
BOOTMGR_DIR = ../bootmgr
UTIL_DIR = ../util
BUILD_DIR = build

# Source files
MBR_SRC = $(MBR_DIR)/mbr.S
VBR_SRC = $(MBR_DIR)/vbr.S
STAGE2_ASM_SRC = $(STAGE2_DIR)/start.S
STAGE2_C_SRC = $(STAGE2_DIR)/stage2.c
HAL_SRC = $(HAL_DIR)/hal.c
FS_SRC = $(FS_DIR)/fs.c
SECURITY_SRC = $(SECURITY_DIR)/verify.c $(SECURITY_DIR)/sha384.c $(SECURITY_DIR)/rsa.c
BOOTMGR_SRC = $(BOOTMGR_DIR)/bootmgr.c
UTIL_SRC = $(UTIL_DIR)/util.c

# Object files
MBR_OBJ = $(BUILD_DIR)/mbr.bin
VBR_OBJ = $(BUILD_DIR)/vbr.bin
STAGE2_ASM_OBJ = $(BUILD_DIR)/start.o
STAGE2_C_OBJ = $(BUILD_DIR)/stage2.o
HAL_OBJ = $(BUILD_DIR)/hal.o
FS_OBJ = $(BUILD_DIR)/fs.o
SECURITY_OBJS = $(BUILD_DIR)/verify.o $(BUILD_DIR)/sha384.o $(BUILD_DIR)/rsa.o
BOOTMGR_OBJ = $(BUILD_DIR)/bootmgr.o
UTIL_OBJ = $(BUILD_DIR)/util.o

# Output files
MBR_OUTPUT = $(BUILD_DIR)/mbr.bin
VBR_OUTPUT = $(BUILD_DIR)/vbr.bin
STAGE2_OUTPUT = $(BUILD_DIR)/stage2.bin
BOOTLOADER_IMAGE = $(BUILD_DIR)/bootloader.img

# Default target
all: directories $(MBR_OUTPUT) $(VBR_OUTPUT) $(STAGE2_OUTPUT) $(BOOTLOADER_IMAGE)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR)

# Build MBR
$(MBR_OUTPUT): $(MBR_SRC)
	@echo "Building MBR..."
	$(AS) $(ASFLAGS) $< -o $@
	@echo "MBR built successfully"

# Build VBR
$(VBR_OUTPUT): $(VBR_SRC)
	@echo "Building VBR..."
	$(AS) $(ASFLAGS) $< -o $@
	@echo "VBR built successfully"

# Build Stage 2 assembly
$(STAGE2_ASM_OBJ): $(STAGE2_ASM_SRC)
	@echo "Building Stage 2 assembly..."
	$(AS) -f elf32 $< -o $@

# Build Stage 2 C code
$(STAGE2_C_OBJ): $(STAGE2_C_SRC)
	@echo "Building Stage 2 C code..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build HAL
$(HAL_OBJ): $(HAL_SRC)
	@echo "Building HAL..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build FS
$(FS_OBJ): $(FS_SRC)
	@echo "Building File System..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build Security
$(BUILD_DIR)/verify.o: $(SECURITY_DIR)/verify.c
	@echo "Building Verify..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sha384.o: $(SECURITY_DIR)/sha384.c
	@echo "Building SHA-384..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/rsa.o: $(SECURITY_DIR)/rsa.c
	@echo "Building RSA..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build Boot Manager
$(BOOTMGR_OBJ): $(BOOTMGR_SRC)
	@echo "Building Boot Manager..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build Utilities
$(UTIL_OBJ): $(UTIL_SRC)
	@echo "Building Utilities..."
	$(CC) $(CFLAGS) -c $< -o $@

# Link Stage 2
$(STAGE2_OUTPUT): $(STAGE2_ASM_OBJ) $(STAGE2_C_OBJ) $(HAL_OBJ) $(FS_OBJ) \
                  $(SECURITY_OBJS) $(BOOTMGR_OBJ) $(UTIL_OBJ)
	@echo "Linking Stage 2..."
	$(LD) $(LDFLAGS) $^ -o $(BUILD_DIR)/stage2.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/stage2.elf $@
	@echo "Stage 2 built successfully"

# Create bootloader image
$(BOOTLOADER_IMAGE): $(MBR_OUTPUT) $(VBR_OUTPUT) $(STAGE2_OUTPUT)
	@echo "Creating bootloader image..."
	dd if=/dev/zero of=$@ bs=512 count=2048 2>/dev/null
	dd if=$(MBR_OUTPUT) of=$@ bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(VBR_OUTPUT) of=$@ bs=512 seek=2048 count=1 conv=notrunc 2>/dev/null
	dd if=$(STAGE2_OUTPUT) of=$@ bs=512 seek=4096 conv=notrunc 2>/dev/null
	@echo "Bootloader image created successfully"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Install to disk
install: all
	@echo "Installing bootloader to disk..."
	@echo "WARNING: This will overwrite the MBR of the specified disk!"
	@read -p "Enter disk device (e.g., /dev/sda): " DISK; \
	dd if=$(MBR_OUTPUT) of=$$DISK bs=512 count=1 conv=notrunc; \
	echo "Bootloader installed to $$DISK"

# Help
help:
	@echo "HIK BIOS Bootloader Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build all components (default)"
	@echo "  mbr       - Build MBR only"
	@echo "  vbr       - Build VBR only"
	@echo "  stage2    - Build Stage 2 only"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install bootloader to disk"
	@echo "  help      - Show this help message"

.PHONY: all directories clean install help