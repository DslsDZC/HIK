#!/bin/bash

# HIK Unified Bootloader Installation Script
# Supports both BIOS and UEFI boot modes

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        print_error "This script must be run as root"
        exit 1
    fi
}

# Check dependencies
check_dependencies() {
    print_step "Checking dependencies..."
    
    local deps=("parted" "mkfs.vfat" "mkfs.ext4" "dd" "grub-install" "efibootmgr")
    local missing=()
    
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing+=("$dep")
        fi
    done
    
    if [ ${#missing[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing[*]}"
        print_info "Please install the missing packages"
        exit 1
    fi
    
    print_info "All dependencies found"
}

# Detect boot mode
detect_boot_mode() {
    print_step "Detecting boot mode..."
    
    if [ -d /sys/firmware/efi ]; then
        BOOT_MODE="uefi"
        print_info "UEFI boot mode detected"
    else
        BOOT_MODE="bios"
        print_info "BIOS boot mode detected"
    fi
}

# Get disk device
get_disk_device() {
    print_step "Selecting disk device..."
    
    # List available disks
    echo "Available disks:"
    lsblk -d -n -o NAME,SIZE,TYPE,MOUNTPOINT | grep disk
    
    echo ""
    read -p "Enter disk device (e.g., /dev/sda): " DISK_DEVICE
    
    if [ ! -b "$DISK_DEVICE" ]; then
        print_error "Invalid disk device: $DISK_DEVICE"
        exit 1
    fi
    
    print_warn "This will erase all data on $DISK_DEVICE!"
    read -p "Are you sure? (yes/no): " CONFIRM
    
    if [ "$CONFIRM" != "yes" ]; then
        print_info "Installation cancelled"
        exit 0
    fi
}

# Create partition table
create_partitions() {
    print_step "Creating partition table..."
    
    # Wipe disk
    wipefs -a "$DISK_DEVICE" || true
    sgdisk --zap-all "$DISK_DEVICE" || true
    
    # Create GPT partition table
    parted "$DISK_DEVICE" mklabel gpt
    
    # Create BIOS boot partition (1MB, type EF02)
    parted "$DISK_DEVICE" mkpart bios_grub 1MiB 2MiB
    parted "$DISK_DEVICE" set 1 bios_grub on
    
    # Create EFI System Partition (100MB, type EF00)
    parted "$DISK_DEVICE" mkpart EFI fat32 2MiB 102MiB
    parted "$DISK_DEVICE" set 2 esp on
    
    # Create HIK partition (remaining, type 8300)
    parted "$DISK_DEVICE" mkpart HIK ext4 102MiB 100%
    
    # Inform kernel of partition changes
    partprobe "$DISK_DEVICE"
    sleep 2
    
    print_info "Partitions created successfully"
}

# Format partitions
format_partitions() {
    print_step "Formatting partitions..."
    
    # Format BIOS boot partition (no filesystem needed)
    print_info "BIOS boot partition: no filesystem"
    
    # Format EFI System Partition
    mkfs.vfat -F 32 "${DISK_DEVICE}p2"
    print_info "EFI System Partition formatted as FAT32"
    
    # Format HIK partition
    mkfs.ext4 "${DISK_DEVICE}p3"
    print_info "HIK partition formatted as ext4"
}

# Mount partitions
mount_partitions() {
    print_step "Mounting partitions..."
    
    # Create mount points
    mkdir -p /mnt/hik/efi
    mkdir -p /mnt/hik/root
    
    # Mount partitions
    mount "${DISK_DEVICE}p2" /mnt/hik/efi
    mount "${DISK_DEVICE}p3" /mnt/hik/root
    
    print_info "Partitions mounted successfully"
}

# Install BIOS bootloader
install_bios_bootloader() {
    print_step "Installing BIOS bootloader..."
    
    # Build BIOS bootloader
    cd ../boot/tools
    make clean
    make
    cd ../../unified
    
    # Install GRUB for BIOS
    grub-install --target=i386-pc \
                 --boot-directory=/mnt/hik/root/boot \
                 --recheck \
                 "$DISK_DEVICE"
    
    # Copy BIOS bootloader
    if [ -f ../boot/build/bootloader.img ]; then
        dd if=../boot/build/bootloader.img of="${DISK_DEVICE}p1" bs=512 conv=notrunc
        print_info "BIOS bootloader installed to BIOS boot partition"
    fi
    
    # Copy kernel to HIK partition
    mkdir -p /mnt/hik/root/HIK
    if [ -f build/kernel.hik ]; then
        cp build/kernel.hik /mnt/hik/root/HIK/
        print_info "Kernel copied to HIK partition"
    fi
    
    # Copy BIOS configuration
    if [ -f ../common/boot.cfg ]; then
        cp ../common/boot.cfg /mnt/hik/root/HIK/
        print_info "BIOS configuration copied"
    fi
}

# Install UEFI bootloader
install_uefi_bootloader() {
    print_step "Installing UEFI bootloader..."
    
    # Build UEFI bootloader
    cd ../uefi/tools
    make clean
    make
    cd ../../unified
    
    # Install GRUB for UEFI
    grub-install --target=x86_64-efi \
                 --efi-directory=/mnt/hik/efi \
                 --bootloader-id=HIK \
                 --recheck \
                 --no-nvram
    
    # Copy UEFI bootloader
    mkdir -p /mnt/hik/efi/EFI/HIK
    if [ -f ../uefi/build/hikboot.efi ]; then
        cp ../uefi/build/hikboot.efi /mnt/hik/efi/EFI/HIK/
        print_info "UEFI bootloader copied to ESP"
    fi
    
    # Copy kernel to ESP
    if [ -f build/kernel.hik ]; then
        cp build/kernel.hik /mnt/hik/efi/EFI/HIK/
        print_info "Kernel copied to ESP"
    fi
    
    # Copy UEFI configuration
    if [ -f ../common/boot.conf ]; then
        cp ../common/boot.conf /mnt/hik/efi/EFI/HIK/
        print_info "UEFI configuration copied"
    fi
    
    # Add UEFI boot entry
    efibootmgr -c -d "$DISK_DEVICE" -p 2 -L "HIK Boot" -l /EFI/HIK/hikboot.efi
    print_info "UEFI boot entry added"
}

# Install both bootloaders
install_both_bootloaders() {
    print_step "Installing both BIOS and UEFI bootloaders..."
    
    # Build both bootloaders
    cd ../boot/tools
    make clean
    make
    cd ../../unified
    
    cd ../uefi/tools
    make clean
    make
    cd ../../unified
    
    # Install GRUB hybrid
    grub-install --target=i386-pc \
                 --boot-directory=/mnt/hik/root/boot \
                 --recheck \
                 "$DISK_DEVICE"
    
    grub-install --target=x86_64-efi \
                 --efi-directory=/mnt/hik/efi \
                 --bootloader-id=HIK \
                 --recheck \
                 --no-nvram
    
    # Copy BIOS bootloader
    if [ -f ../boot/build/bootloader.img ]; then
        dd if=../boot/build/bootloader.img of="${DISK_DEVICE}p1" bs=512 conv=notrunc
        print_info "BIOS bootloader installed"
    fi
    
    # Copy UEFI bootloader
    mkdir -p /mnt/hik/efi/EFI/HIK
    if [ -f ../uefi/build/hikboot.efi ]; then
        cp ../uefi/build/hikboot.efi /mnt/hik/efi/EFI/HIK/
        print_info "UEFI bootloader copied"
    fi
    
    # Copy kernel to both partitions
    mkdir -p /mnt/hik/root/HIK
    if [ -f build/kernel.hik ]; then
        cp build/kernel.hik /mnt/hik/root/HIK/
        cp build/kernel.hik /mnt/hik/efi/EFI/HIK/
        print_info "Kernel copied to both partitions"
    fi
    
    # Copy configurations
    if [ -f ../common/boot.cfg ]; then
        cp ../common/boot.cfg /mnt/hik/root/HIK/
    fi
    if [ -f ../common/boot.conf ]; then
        cp ../common/boot.conf /mnt/hik/efi/EFI/HIK/
    fi
    print_info "Configurations copied"
    
    # Add UEFI boot entry
    efibootmgr -c -d "$DISK_DEVICE" -p 2 -L "HIK Boot" -l /EFI/HIK/hikboot.efi
    print_info "UEFI boot entry added"
}

# Unmount partitions
unmount_partitions() {
    print_step "Unmounting partitions..."
    
    sync
    umount /mnt/hik/efi
    umount /mnt/hik/root
    
    # Remove mount points
    rmdir /mnt/hik/efi
    rmdir /mnt/hik/root
    rmdir /mnt/hik
    
    print_info "Partitions unmounted successfully"
}

# Main installation function
main() {
    echo ""
    echo "========================================"
    echo "  HIK Unified Bootloader Installation"
    echo "========================================"
    echo ""
    
    check_root
    check_dependencies
    detect_boot_mode
    
    # Ask for boot mode
    echo ""
    echo "Select boot mode:"
    echo "  1) BIOS only"
    echo "  2) UEFI only"
    echo "  3) Both BIOS and UEFI (recommended)"
    echo ""
    read -p "Enter choice [1-3]: " BOOT_CHOICE
    
    case $BOOT_CHOICE in
        1)
            INSTALL_MODE="bios"
            ;;
        2)
            INSTALL_MODE="uefi"
            ;;
        3)
            INSTALL_MODE="both"
            ;;
        *)
            print_error "Invalid choice"
            exit 1
            ;;
    esac
    
    get_disk_device
    create_partitions
    format_partitions
    mount_partitions
    
    case $INSTALL_MODE in
        bios)
            install_bios_bootloader
            ;;
        uefi)
            install_uefi_bootloader
            ;;
        both)
            install_both_bootloaders
            ;;
    esac
    
    unmount_partitions
    
    echo ""
    print_info "Installation completed successfully!"
    echo ""
    echo "Boot mode: $INSTALL_MODE"
    echo "Disk: $DISK_DEVICE"
    echo ""
    echo "You can now reboot your system."
}

# Run main function
main "$@"