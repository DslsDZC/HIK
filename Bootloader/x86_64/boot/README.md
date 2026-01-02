# HIK BIOS Bootloader

HIK (Hierarchical Isolation Kernel) BIOS Bootloader - A traditional BIOS-compatible bootloader for the HIK operating system.

## Overview

The HIK BIOS Bootloader provides a traditional BIOS-compatible boot experience for the HIK kernel. It supports legacy BIOS systems and implements modern boot practices including:

- **Multi-stage Boot**: MBR → VBR → Stage 2 → Kernel
- **Hardware Detection**: Memory map, ACPI tables, VESA graphics
- **File System Support**: FAT32 file system for kernel loading
- **Digital Signature Verification**: RSA-3072 + SHA-384 signature support
- **Protected Mode**: 32-bit protected mode with long mode transition
- **Boot Configuration**: Simple configuration file support

## Architecture

The bootloader is organized into several functional layers:

### 1. MBR (Master Boot Record) (`mbr/`)
- First 512 bytes of disk
- Loads VBR from active partition
- Switches to protected mode
- Relocates itself to 0x6000

### 2. VBR (Volume Boot Record) (`mbr/vbr.S`)
- Loads Stage 2 bootloader
- Initializes protected mode environment
- Located at sector 2048 (1MB offset)

### 3. Stage 2 Bootloader (`stage2/`)
- Main bootloader logic in 32-bit protected mode
- Hardware initialization
- Kernel loading and verification
- Long mode transition

### 4. Hardware Abstraction Layer (`hal/`)
- VGA text mode output
- Serial port communication
- Memory detection via BIOS INT 15h
- ACPI table detection
- System reboot

### 5. File System Layer (`fs/`)
- FAT32 file system support
- File operations (open, read, seek)
- Directory listing

### 6. Security Layer (`security/`)
- SHA-384 hash implementation
- RSA signature verification
- Kernel image verification

### 7. Boot Manager (`bootmgr/`)
- Boot configuration parsing
- Boot menu display
- Default boot entry selection

### 8. Utilities (`util/`)
- String functions
- Memory operations
- Port I/O
- Number conversion

### 9. Build Tools (`tools/`)
- Makefile for automated building
- Shell build script
- Linker script
- Build documentation

## Building

### Prerequisites

You need the following tools installed:

- **GCC**: GNU C Compiler with 32-bit support
- **NASM**: Netwide Assembler
- **Binutils**: GNU Binary Utilities (ld, objcopy)
- **Make**: Build automation tool
- **DD**: Disk duplication utility

#### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install build-essential nasm binutils
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc nasm binutils make
```

**Arch Linux:**
```bash
sudo pacman -S base-devel nasm binutils
```

### Quick Start

```bash
cd Bootloader/x86_64/boot
chmod +x tools/build.sh
./tools/build.sh build
```

The output will be:
- `build/mbr.bin` - Master Boot Record (512 bytes)
- `build/vbr.bin` - Volume Boot Record (512 bytes)
- `build/stage2.bin` - Stage 2 Bootloader
- `build/bootloader.img` - Complete bootloader image (1MB)

### Using Makefile

```bash
cd Bootloader/x86_64/boot/tools
make          # Build all components
make clean    # Remove build artifacts
make install  # Install to disk (requires sudo)
make help     # Show help
```

## Installation

### Installing to Disk

**WARNING: This will overwrite the MBR of the specified disk! Make sure you have a backup!**

1. **Prepare the disk:**
   - Create a FAT32 partition
   - Copy kernel to `/HIK/kernel.hik` on the partition
   - Mark the partition as active (bootable)

2. **Install MBR:**
   ```bash
   sudo dd if=build/mbr.bin of=/dev/sdX bs=512 count=1
   ```

3. **Install VBR:**
   ```bash
   sudo dd if=build/vbr.bin of=/dev/sdX bs=512 seek=2048 count=1
   ```

4. **Install Stage 2:**
   ```bash
   sudo dd if=build/stage2.bin of=/dev/sdX bs=512 seek=4096
   ```

Replace `/dev/sdX` with your actual disk device (e.g., `/dev/sda`).

### Using the Build Script

The build script provides an install target:

```bash
./tools/build.sh install
```

This will prompt you for the disk device and install the bootloader.

## Kernel Image Format

The HIK kernel image format is compatible with the UEFI version:

```c
typedef struct {
    uint64_t signature;          // HIK_KERNEL_MAGIC (0x48494B00)
    uint32_t version;            // Kernel version
    uint32_t flags;              // Flags (e.g., HIK_FLAG_SIGNED)
    uint64_t entry_point;        // Entry point offset
    uint64_t code_offset;        // Code section offset
    uint64_t code_size;          // Code section size
    uint64_t data_offset;        // Data section offset
    uint64_t data_size;          // Data section size
    uint64_t config_offset;      // Config section offset
    uint64_t config_size;        // Config section size
    uint64_t signature_offset;   // Signature section offset
    uint64_t signature_size;     // Signature section size
    uint8_t  reserved[32];       // Reserved for future use
} kernel_header_t;
```

## Boot Configuration

The bootloader supports a simple configuration file located at `/HIK/boot.cfg`:

```
title = "HIK Boot Manager"
timeout = 5

[entry]
name = "HIK Kernel"
kernel = "/HIK/kernel.hik"
initrd = "/HIK/initrd.img"
args = "console=ttyS0,115200"
default = true
```

### Configuration Options

- `title`: Boot menu title
- `timeout`: Default boot timeout in seconds
- `[entry]`: Defines a boot entry
  - `name`: Display name for the entry
  - `kernel`: Path to kernel image
  - `initrd`: Path to initrd image (optional)
  - `args`: Kernel command line arguments
  - `default`: Whether this is the default entry

## Boot Information Passed to Kernel

The bootloader constructs a `hik_boot_info_t` structure and passes its address to the kernel:

```c
typedef struct {
    uint32_t magic;              // "HIK!"
    uint32_t version;            // Structure version
    uint64_t flags;              // Feature flags
    
    // Memory information
    uint64_t memory_map_base;
    uint64_t memory_map_size;
    uint64_t memory_map_desc_size;
    uint32_t memory_map_count;
    
    // ACPI information
    uint64_t rsdp;               // ACPI RSDP address
    
    // BIOS information
    uint64_t bios_data_area;     // BDA pointer
    uint32_t vbe_info;           // VESA info block
    
    // Kernel information
    uint64_t kernel_base;
    uint64_t kernel_size;
    uint64_t entry_point;
    
    // Command line
    char cmdline[256];
    
    // Module information
    uint64_t modules;
    uint32_t module_count;
} hik_boot_info_t;
```

## Memory Layout

```
0x00000000 - 0x000004FF  BIOS Data Area
0x00000500 - 0x00000FFF  Reserved (Disk Address Packet)
0x00006000 - 0x00007DFF  MBR (relocated)
0x00007C00 - 0x00007DFF  VBR (original location)
0x00009000 - 0x00094FFF  Boot Info Structure
0x00009500 - 0x00009FFF  Memory Map
0x00010000 - 0x00017FFF  Stage 2 Bootloader (32KB)
0x00080000 - 0x000FFFFF  FAT Table (cached)
0x00090000 - 0x000FFFFF  File Data Buffer
0x00100000 - 0x0FFFFFFF  Kernel Image (up to 240MB)
0x100000000+           Long mode kernel execution
```

## Security Features

### Signature Verification

The bootloader supports RSA-3072 signature verification with SHA-384 hashing:

- **Algorithm**: RSA-3072 with PKCS#1 v2.1 padding
- **Hash**: SHA-384
- **Public Key**: Embedded in bootloader (384 bytes)
- **Verification**: Performed before kernel execution

### Secure Boot Considerations

Since BIOS does not have native Secure Boot support, the bootloader implements its own signature verification:

1. Kernel must be signed with the embedded public key
2. Signature is verified before kernel execution
3. Invalid signatures prevent kernel boot

## Debugging

### Serial Console

The bootloader outputs debug information to COM1 (0x3F8) at 115200 baud:

```bash
# Connect to serial console
sudo screen /dev/ttyS0 115200
```

### VGA Output

Debug information is also displayed on VGA text mode (80x25):

- White text on black background
- Auto-scrolling
- Error messages in red

### Common Issues

**Bootloader doesn't load:**
- Check disk partition table (must have active partition)
- Verify MBR signature (0x55AA)
- Check BIOS boot order

**Kernel not found:**
- Verify kernel path in configuration
- Check FAT32 file system integrity
- Ensure kernel is in `/HIK/` directory

**Signature verification fails:**
- Check kernel is properly signed
- Verify public key matches
- Check signature format

## Testing

### QEMU Testing

Test the bootloader with QEMU:

```bash
# Create test disk image
dd if=/dev/zero of=test.img bs=1M count=64

# Create partition table and FAT32 partition
parted test.img mklabel msdos
parted test.img mkpart primary fat32 1MiB 100%
parted test.img set 1 boot on

# Format partition
sudo losetup /dev/loop0 test.img
sudo losetup /dev/loop1 test.img -o 1048576
sudo mkfs.vfat -F 32 /dev/loop1

# Mount and copy files
sudo mount /dev/loop1 /mnt
sudo mkdir -p /mnt/HIK
sudo cp build/stage2.bin /mnt/HIK/kernel.hik
sudo umount /mnt

# Install bootloader
sudo dd if=build/mbr.bin of=/dev/loop0 bs=512 count=1
sudo dd if=build/vbr.bin of=/dev/loop0 bs=512 seek=2048 count=1
sudo dd if=build/stage2.bin of=/dev/loop0 bs=512 seek=4096

# Cleanup
sudo losetup -d /dev/loop0 /dev/loop1

# Test with QEMU
qemu-system-x86_64 -drive file=test.img,format=raw -serial stdio
```

### Physical Hardware Testing

1. Use a spare disk or USB drive
2. Install bootloader as described above
3. Set BIOS to boot from the disk
4. Monitor serial console for debug output

## Troubleshooting

### Build Errors

**"nasm: command not found":**
- Install NASM: `sudo apt-get install nasm`

**"ld: unrecognized option '-m elf_i386'":**
- Ensure you have 32-bit binutils: `sudo apt-get install binutils:i386`

**Compilation errors:**
- Check GCC version (4.8+ recommended)
- Verify all dependencies are installed
- Check CFLAGS in Makefile

### Runtime Errors

**"Invalid kernel magic":**
- Check kernel file format
- Verify kernel is properly built

**"Signature verification failed":**
- Check kernel signature
- Verify public key is correct

**"File system initialization failed":**
- Check partition is FAT32
- Verify partition is marked active
- Check disk geometry

## Development

### Code Style

- Use 4-space indentation
- Maximum line length: 100 characters
- Functions should be short and focused
- Add comments for complex logic
- Use meaningful variable names

### Adding Features

1. Identify the appropriate layer for the feature
2. Follow existing code style and conventions
3. Add necessary error handling
4. Update documentation
5. Test thoroughly

### Submitting Changes

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## Comparison with UEFI Bootloader

| Feature | BIOS Bootloader | UEFI Bootloader |
|---------|----------------|-----------------|
| Boot Mode | Traditional BIOS | UEFI |
| Boot Time | Slower (BIOS INT 13h) | Faster (UEFI protocols) |
| Graphics | VGA text mode | Frame buffer support |
| Security | Custom verification | UEFI Secure Boot |
| File System | FAT32 only | Multiple (FAT32, ext2, etc.) |
| Memory Map | BIOS INT 15h | UEFI memory map |
| ACPI | Manual detection | UEFI ACPI tables |
| Compatibility | Legacy systems | Modern systems |

## Future Enhancements

Potential future improvements:

1. **Graphics Support**: VESA VBE mode switching
2. **Network Boot**: PXE support for network booting
3. **Multi-boot**: Support for loading other OS kernels
4. **Compression**: Kernel image compression support
5. **Recovery Mode**: Built-in recovery console
6. **TPM Integration**: TPM 1.2/2.0 support
7. **USB Boot**: Improved USB device support
8. **GPT Support**: GPT partition table support

## License

This bootloader is part of the HIK (Hierarchical Isolation Kernel) project.

## References

- [BIOS Boot Specification](https://www.dig64.org/specifications/DIG64_PDR_0.95.pdf)
- [FAT32 Specification](https://staff.washington.edu/dittrich/misc/fatgen103.pdf)
- [x86 Architecture](https://www.intel.com/content/www/us/en/architecture-and-technology/64-ia-32-architectures-software-developer-manual-325462.html)
- [HIK Architecture Documentation](../../../../三层模型.md)
- [Bootloader Design](../../../../引导加载程序.md)

## Contributing

Contributions are welcome! Please ensure:
- Code follows existing style and conventions
- Changes are well-documented
- Testing is performed
- Build system is updated if needed

## Contact

For issues and questions, please refer to the main HIK project documentation.

---

**Last Updated**: 2026-01-02
**Version**: 1.0
**Status**: Complete ✅