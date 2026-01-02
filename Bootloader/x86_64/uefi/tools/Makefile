# HIK UEFI Bootloader Makefile

# Compiler and tools
CC = gcc
AS = nasm
LD = ld
OBJCOPY = objcopy
EIFTOOL = efi-to-elf

# Directories
ROOT_DIR = ../../..
BUILD_DIR = build
SRC_DIR = ..
EFI_DIR = $(SRC_DIR)/efi
HAL_DIR = $(SRC_DIR)/hal
FS_DIR = $(SRC_DIR)/fs
SECURITY_DIR = $(SRC_DIR)/security
BOOTMGR_DIR = $(SRC_DIR)/bootmgr
UTIL_DIR = $(SRC_DIR)/util

# Compiler flags
CFLAGS = -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar \
         -mno-red-zone -maccumulate-outgoing-args -DGNU_EFI_USE_MS_ABI \
         -I$(EFI_DIR) -I$(HAL_DIR) -I$(FS_DIR) -I$(SECURITY_DIR) -I$(BOOTMGR_DIR) -I$(UTIL_DIR) \
         -Wall -Wextra -O2 -g

ASFLAGS = -f elf64

LDFLAGS = -nostdlib -znocombreloc -T $(SRC_DIR)/linker.ld

# Source files
EFI_SOURCES = $(EFI_DIR)/efi.c $(EFI_DIR)/string.c
HAL_SOURCES = $(HAL_DIR)/gdt.c $(HAL_DIR)/idt.c $(HAL_DIR)/paging.c \
              $(HAL_DIR)/memory.c $(HAL_DIR)/acpi.c $(HAL_DIR)/cpu.c $(HAL_DIR)/jump.c
FS_SOURCES = $(FS_DIR)/volume.c $(FS_DIR)/config.c
SECURITY_SOURCES = $(SECURITY_DIR)/sha384.c $(SECURITY_DIR)/rsa.c $(SECURITY_DIR)/verify.c
BOOTMGR_SOURCES = $(BOOTMGR_DIR)/bootmgr.c $(BOOTMGR_DIR)/main.c
UTIL_SOURCES = $(UTIL_DIR)/util.c

SOURCES = $(EFI_SOURCES) $(HAL_SOURCES) $(FS_SOURCES) $(SECURITY_SOURCES) \
          $(BOOTMGR_SOURCES) $(UTIL_SOURCES)

OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Output files
BOOTLOADER_EFI = $(BUILD_DIR)/hikboot.efi
BOOTLOADER_SO = $(BUILD_DIR)/hikboot.so

# Default target
all: $(BOOTLOADER_EFI)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/efi
	mkdir -p $(BUILD_DIR)/hal
	mkdir -p $(BUILD_DIR)/fs
	mkdir -p $(BUILD_DIR)/security
	mkdir -p $(BUILD_DIR)/bootmgr
	mkdir -p $(BUILD_DIR)/util

# Compile C files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link
$(BOOTLOADER_SO): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^ -lefi -lgnuefi

# Create EFI file
$(BOOTLOADER_EFI): $(BOOTLOADER_SO)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
	           -j .rela -j .reloc --target=efi-app-x86_64 $< $@

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Install
install: $(BOOTLOADER_EFI)
	@echo "Please copy $(BOOTLOADER_EFI) to your EFI system partition"
	@echo "Typically: sudo cp $(BOOTLOADER_EFI) /boot/efi/EFI/HIK/"

.PHONY: all clean install