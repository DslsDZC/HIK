#!/bin/bash

# HIK BIOS Bootloader Build Script

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Print functions
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check dependencies
check_dependencies() {
    print_info "Checking dependencies..."
    
    local deps=("gcc" "nasm" "ld" "objcopy" "dd")
    local missing=()
    
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing+=("$dep")
        fi
    done
    
    if [ ${#missing[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing[*]}"
        print_info "Please install the missing packages:"
        print_info "  Ubuntu/Debian: sudo apt-get install build-essential nasm binutils"
        print_info "  Fedora/RHEL:   sudo dnf install gcc nasm binutils"
        print_info "  Arch Linux:    sudo pacman -S base-devel nasm binutils"
        exit 1
    fi
    
    print_info "All dependencies found"
}

# Build MBR
build_mbr() {
    print_info "Building MBR..."
    
    cd mbr
    nasm -f bin mbr.S -o ../build/mbr.bin || {
        print_error "Failed to build MBR"
        exit 1
    }
    cd ..
    
    print_info "MBR built successfully"
}

# Build VBR
build_vbr() {
    print_info "Building VBR..."
    
    cd mbr
    nasm -f bin vbr.S -o ../build/vbr.bin || {
        print_error "Failed to build VBR"
        exit 1
    }
    cd ..
    
    print_info "VBR built successfully"
}

# Build Stage 2
build_stage2() {
    print_info "Building Stage 2..."
    
    # Compile C files
    print_info "  Compiling C files..."
    
    gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -O2 \
        -Imbr -Istage2 -Ihal -Ifs -Isecurity -Iutil \
        -c stage2/stage2.c -o build/stage2.o || {
        print_error "Failed to compile stage2.c"
        exit 1
    }
    
    gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -O2 \
        -Imbr -Istage2 -Ihal -Ifs -Isecurity -Iutil \
        -c hal/hal.c -o build/hal.o || {
        print_error "Failed to compile hal.c"
        exit 1
    }
    
    gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -O2 \
        -Imbr -Istage2 -Ihal -Ifs -Isecurity -Iutil \
        -c fs/fs.c -o build/fs.o || {
        print_error "Failed to compile fs.c"
        exit 1
    }
    
    gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -O2 \
        -Imbr -Istage2 -Ihal -Ifs -Isecurity -Iutil \
        -c security/verify.c -o build/verify.o || {
        print_error "Failed to compile verify.c"
        exit 1
    }
    
    gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -O2 \
        -Imbr -Istage2 -Ihal -Ifs -Isecurity -Iutil \
        -c security/sha384.c -o build/sha384.o || {
        print_error "Failed to compile sha384.c"
        exit 1
    }
    
    gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -O2 \
        -Imbr -Istage2 -Ihal -Ifs -Isecurity -Iutil \
        -c security/rsa.c -o build/rsa.o || {
        print_error "Failed to compile rsa.c"
        exit 1
    }
    
    gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -O2 \
        -Imbr -Istage2 -Ihal -Ifs -Isecurity -Iutil \
        -c bootmgr/bootmgr.c -o build/bootmgr.o || {
        print_error "Failed to compile bootmgr.c"
        exit 1
    }
    
    gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
        -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -O2 \
        -Imbr -Istage2 -Ihal -Ifs -Isecurity -Iutil \
        -c util/util.c -o build/util.o || {
        print_error "Failed to compile util.c"
        exit 1
    }
    
    # Assemble assembly files
    print_info "  Assembling assembly files..."
    
    nasm -f elf32 stage2/start.S -o build/start.o || {
        print_error "Failed to assemble start.S"
        exit 1
    }
    
    # Link Stage 2
    print_info "  Linking Stage 2..."
    
    ld -m elf_i386 -T tools/linker.ld -nostdlib \
        build/start.o build/stage2.o build/hal.o build/fs.o \
        build/verify.o build/sha384.o build/rsa.o \
        build/bootmgr.o build/util.o \
        -o build/stage2.elf || {
        print_error "Failed to link Stage 2"
        exit 1
    }
    
    # Convert to binary
    objcopy -O binary build/stage2.elf build/stage2.bin || {
        print_error "Failed to convert Stage 2 to binary"
        exit 1
    }
    
    print_info "Stage 2 built successfully"
}

# Create bootloader image
create_image() {
    print_info "Creating bootloader image..."
    
    # Create 1MB image
    dd if=/dev/zero of=build/bootloader.img bs=1M count=1 2>/dev/null || {
        print_error "Failed to create image file"
        exit 1
    }
    
    # Write MBR
    dd if=build/mbr.bin of=build/bootloader.img bs=512 count=1 conv=notrunc 2>/dev/null || {
        print_error "Failed to write MBR"
        exit 1
    }
    
    # Write VBR at sector 2048 (1MB)
    dd if=build/vbr.bin of=build/bootloader.img bs=512 seek=2048 count=1 conv=notrunc 2>/dev/null || {
        print_error "Failed to write VBR"
        exit 1
    }
    
    # Write Stage 2 at sector 4096 (2MB)
    dd if=build/stage2.bin of=build/bootloader.img bs=512 seek=4096 conv=notrunc 2>/dev/null || {
        print_error "Failed to write Stage 2"
        exit 1
    }
    
    print_info "Bootloader image created successfully"
}

# Main build function
build() {
    print_info "Building HIK BIOS Bootloader..."
    echo ""
    
    # Create build directory
    mkdir -p build
    
    # Build components
    build_mbr
    build_vbr
    build_stage2
    
    # Create image
    create_image
    
    echo ""
    print_info "Build completed successfully!"
    print_info "Output files:"
    print_info "  build/mbr.bin - Master Boot Record (512 bytes)"
    print_info "  build/vbr.bin - Volume Boot Record (512 bytes)"
    print_info "  build/stage2.bin - Stage 2 Bootloader"
    print_info "  build/bootloader.img - Complete bootloader image (1MB)"
}

# Clean function
clean() {
    print_info "Cleaning build artifacts..."
    rm -rf build
    print_info "Clean complete"
}

# Help function
help() {
    echo "HIK BIOS Bootloader Build Script"
    echo ""
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  build    - Build all components (default)"
    echo "  clean    - Remove build artifacts"
    echo "  help     - Show this help message"
}

# Main script
case "${1:-build}" in
    build)
        check_dependencies
        build
        ;;
    clean)
        clean
        ;;
    help|--help|-h)
        help
        ;;
    *)
        print_error "Unknown command: $1"
        help
        exit 1
        ;;
esac