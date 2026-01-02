#!/bin/bash

# HIK UEFI Bootloader Build Script

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
EFI_DIR="$SCRIPT_DIR/efi"

# Check for required tools
check_tools() {
    echo -e "${YELLOW}Checking for required tools...${NC}"
    
    local tools=("gcc" "nasm" "ld" "objcopy")
    local missing_tools=()
    
    for tool in "${tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            missing_tools+=("$tool")
        fi
    done
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        echo -e "${RED}Error: Missing required tools: ${missing_tools[*]}${NC}"
        echo "Please install the missing tools and try again."
        exit 1
    fi
    
    echo -e "${GREEN}All required tools found.${NC}"
}

# Clean build directory
clean() {
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}Clean completed.${NC}"
}

# Build the bootloader
build() {
    echo -e "${YELLOW}Building HIK UEFI Bootloader...${NC}"
    
    # Create build directories
    mkdir -p "$BUILD_DIR/efi"
    mkdir -p "$BUILD_DIR/hal"
    mkdir -p "$BUILD_DIR/fs"
    mkdir -p "$BUILD_DIR/security"
    mkdir -p "$BUILD_DIR/bootmgr"
    mkdir -p "$BUILD_DIR/util"
    
    # Compile C files
    echo "Compiling C files..."
    
    # EFI layer
    gcc -c efi/efi.c -o "$BUILD_DIR/efi/efi.o" \
        -ffreestanding -fno-stack-protector -fshort-wchar \
        -mno-red-zone -maccumulate-outgoing-args \
        -I"$EFI_DIR" -Wall -Wextra -O2 -g
    
    gcc -c efi/string.c -o "$BUILD_DIR/efi/string.o" \
        -ffreestanding -fno-stack-protector -fshort-wchar \
        -mno-red-zone -maccumulate-outgoing-args \
        -I"$EFI_DIR" -Wall -Wextra -O2 -g
    
    # HAL layer
    for file in hal/gdt.c hal/idt.c hal/paging.c hal/memory.c hal/acpi.c hal/cpu.c hal/jump.c; do
        gcc -c "$file" -o "$BUILD_DIR/$(basename "$file" .c).o" \
            -ffreestanding -fno-stack-protector \
            -mno-red-zone -maccumulate-outgoing-args \
            -I"$EFI_DIR" -I"hal" -Wall -Wextra -O2 -g
    done
    
    # File system
    for file in fs/volume.c fs/config.c; do
        gcc -c "$file" -o "$BUILD_DIR/$(basename "$file" .c).o" \
            -ffreestanding -fno-stack-protector \
            -mno-red-zone -maccumulate-outgoing-args \
            -I"$EFI_DIR" -I"fs" -Wall -Wextra -O2 -g
    done
    
    # Security
    for file in security/sha384.c security/rsa.c security/verify.c; do
        gcc -c "$file" -o "$BUILD_DIR/$(basename "$file" .c).o" \
            -ffreestanding -fno-stack-protector \
            -mno-red-zone -maccumulate-outgoing-args \
            -I"$EFI_DIR" -I"security" -Wall -Wextra -O2 -g
    done
    
    # Boot manager
    for file in bootmgr/bootmgr.c bootmgr/main.c; do
        gcc -c "$file" -o "$BUILD_DIR/$(basename "$file" .c).o" \
            -ffreestanding -fno-stack-protector \
            -mno-red-zone -maccumulate-outgoing-args \
            -I"$EFI_DIR" -I"hal" -I"fs" -I"security" -I"bootmgr" -I"util" \
            -Wall -Wextra -O2 -g
    done
    
    # Utilities
    gcc -c util/util.c -o "$BUILD_DIR/util/util.o" \
        -ffreestanding -fno-stack-protector \
        -mno-red-zone -maccumulate-outgoing-args \
        -I"$EFI_DIR" -I"hal" -I"util" -Wall -Wextra -O2 -g
    
    # Link
    echo "Linking..."
    ld -nostdlib -znocombreloc -T tools/linker.ld \
        -o "$BUILD_DIR/hikboot.so" \
        $(find "$BUILD_DIR" -name "*.o") \
        -lefi -lgnuefi
    
    # Create EFI file
    echo "Creating EFI file..."
    objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
            -j .rela -j .reloc --target=efi-app-x86_64 \
            "$BUILD_DIR/hikboot.so" "$BUILD_DIR/hikboot.efi"
    
    echo -e "${GREEN}Build completed successfully!${NC}"
    echo "Output: $BUILD_DIR/hikboot.efi"
}

# Install the bootloader
install() {
    if [ ! -f "$BUILD_DIR/hikboot.efi" ]; then
        echo -e "${RED}Error: Bootloader not built. Run 'build' first.${NC}"
        exit 1
    fi
    
    echo -e "${YELLOW}Installing HIK UEFI Bootloader...${NC}"
    echo "Please copy $BUILD_DIR/hikboot.efi to your EFI system partition"
    echo "Typically: sudo cp $BUILD_DIR/hikboot.efi /boot/efi/EFI/HIK/"
}

# Main script
case "${1:-build}" in
    clean)
        clean
        ;;
    build)
        check_tools
        build
        ;;
    install)
        install
        ;;
    rebuild)
        clean
        check_tools
        build
        ;;
    *)
        echo "Usage: $0 {clean|build|install|rebuild}"
        echo ""
        echo "Commands:"
        echo "  clean    - Remove build artifacts"
        echo "  build    - Build the bootloader"
        echo "  install  - Show installation instructions"
        echo "  rebuild  - Clean and build"
        exit 1
        ;;
esac