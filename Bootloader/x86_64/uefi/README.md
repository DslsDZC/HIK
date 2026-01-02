# HIK UEFI Bootloader

HIK (Hierarchical Isolation Kernel) UEFI Bootloader - A modern, secure bootloader for the HIK operating system.

## Overview

The HIK UEFI Bootloader is designed to provide a secure, efficient, and feature-rich boot experience for the HIK kernel. It follows the UEFI specification and implements modern boot practices including:

- **Secure Boot Support**: Integration with UEFI Secure Boot for verified boot chain
- **Digital Signature Verification**: RSA-3072 + SHA-384 signature verification
- **Multi-boot Configuration**: Support for multiple kernel configurations
- **Hardware Abstraction**: Clean HAL layer for x86_64 architecture
- **Memory Management**: Proper UEFI memory map handling and page table setup
- **ACPI Support**: Detection and passing of ACPI tables to the kernel

## Architecture

The bootloader is organized into several functional layers:

### 1. EFI Layer (`efi/`)
- UEFI protocol definitions and type declarations
- UEFI system table and boot services abstraction
- String manipulation utilities
- Basic I/O operations

### 2. Hardware Abstraction Layer (`hal/`)
- GDT (Global Descriptor Table) setup
- IDT (Interrupt Descriptor Table) setup
- Page table initialization for long mode
- Memory map acquisition
- ACPI table detection
- CPU information gathering
- Kernel jump function

### 3. File System Layer (`fs/`)
- UEFI Simple File System Protocol wrapper
- Volume and file operations
- Boot configuration file parser
- Directory listing support

### 4. Security Layer (`security/`)
- SHA-384 hash implementation
- RSA signature verification
- Kernel image verification
- Secure Boot status checking

### 5. Boot Manager (`bootmgr/`)
- Boot menu display and user interaction
- Kernel loading and relocation
- Boot information structure construction
- Kernel entry point setup
- Boot service exit and kernel jump

### 6. Utilities (`util/`)
- Hex dump utilities
- Memory map display
- CPU information display
- Alignment and bit manipulation functions

### 7. Build Tools (`tools/`)
- Makefile for automated building
- Shell build script
- Linker script
- Build documentation

## Building

### Quick Start

```bash
cd Bootloader/x86_64/uefi
chmod +x tools/build.sh
./tools/build.sh build
```

The output will be `build/hikboot.efi`.

### Detailed Instructions

See `tools/README.md` for detailed build instructions, including:
- Prerequisites and required tools
- Installation instructions for various Linux distributions
- Troubleshooting guide

## Installation

1. **Create EFI Directory Structure**:
```bash
sudo mkdir -p /boot/efi/EFI/HIK
```

2. **Copy Bootloader**:
```bash
sudo cp build/hikboot.efi /boot/efi/EFI/HIK/
```

3. **Create Configuration File**:
Copy `boot.conf.example` to `/boot/efi/EFI/HIK/boot.conf` and modify as needed.

4. **Add to EFI Boot Entries**:
```bash
sudo efibootmgr -c -d /dev/sda -p 1 -L "HIK Boot" -l /EFI/HIK/hikboot.efi
```

## Kernel Image Format

The HIK kernel image format includes:

```c
typedef struct {
    UINT64  Signature;        // HIK_KERNEL_MAGIC (0x48494B00)
    UINT32  Version;          // Kernel version
    UINT32  Flags;            // Flags (e.g., HIK_FLAG_SIGNED)
    UINT64  EntryPoint;       // Kernel entry point address
    UINT64  CodeOffset;       // Offset to code section
    UINT64  CodeSize;         // Size of code section
    UINT64  DataOffset;       // Offset to data section
    UINT64  DataSize;         // Size of data section
    UINT64  ConfigOffset;     // Offset to configuration section
    UINT64  ConfigSize;       // Size of configuration section
    UINT64  SignatureOffset;  // Offset to signature section
    UINT64  SignatureSize;    // Size of signature section
    UINT8   Reserved[32];     // Reserved for future use
} HIK_KERNEL_HEADER;
```

## Boot Configuration

The boot configuration file (`boot.conf`) uses a simple INI-like format:

```
title = "HIK Boot Manager"
timeout = 5

[entry]
name = "HIK Kernel"
kernel = "\\EFI\\HIK\\kernel.hik"
initrd = "\\EFI\\HIK\\initrd.img"
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

The bootloader constructs a `HIK_BOOT_INFO` structure and passes its address to the kernel:

```c
typedef struct {
    UINT64  MemoryMapBase;              // Physical address of memory map
    UINT64  MemoryMapSize;              // Size of memory map
    UINT64  MemoryMapDescriptorSize;    // Size of each descriptor
    UINT32  MemoryMapDescriptorVersion; // Descriptor version
    UINT64  AcpiTable;                  // ACPI RSDP address
    UINT64  SmbiosTable;                // SMBIOS table address
    UINT64  SystemTable;                // UEFI System Table address
    UINT64  FrameBufferBase;            // Frame buffer physical address
    UINT64  FrameBufferSize;            // Frame buffer size
    UINT32  HorizontalResolution;       // Screen width
    UINT32  VerticalResolution;         // Screen height
    UINT32  PixelFormat;                // Pixel format
    UINT32  Reserved;                   // Reserved
} HIK_BOOT_INFO;
```

## Security Features

### Secure Boot

The bootloader checks the UEFI Secure Boot status and can be configured to:
- Verify kernel signatures when Secure Boot is enabled
- Allow unsigned kernels when Secure Boot is disabled
- Provide clear error messages for verification failures

### Signature Verification

- Algorithm: RSA-3072 with SHA-384
- Verification covers: Kernel header and entire image
- Public key management: Configurable via build-time options

## Development

### Code Organization

- Each layer is independent and well-encapsulated
- Clear separation between platform-specific and generic code
- Minimal dependencies between layers
- Comprehensive error handling

### Adding Features

1. Identify the appropriate layer for the feature
2. Follow existing code style and conventions
3. Add necessary error handling
4. Update documentation
5. Test thoroughly

### Debugging

The bootloader provides:
- Console output via UEFI ConOut
- Hex dump utilities for memory inspection
- Memory map display
- CPU information display
- Detailed error messages

## Troubleshooting

### Boot Fails to Start

1. Check EFI file placement
2. Verify boot configuration file format
3. Check UEFI boot order
4. Enable verbose output

### Signature Verification Fails

1. Verify kernel is properly signed
2. Check public key configuration
3. Ensure Secure Boot is correctly configured

### Build Errors

1. Verify all required tools are installed
2. Check compiler version compatibility
3. Ensure sufficient disk space
4. Review build log for specific errors

## License

This bootloader is part of the HIK (Hierarchical Isolation Kernel) project.

## References

- [UEFI Specification](https://uefi.org/specifications)
- [HIK Architecture Documentation](../../../三层模型.md)
- [Bootloader Design](../../../引导加载程序.md)

## Contributing

Contributions are welcome! Please ensure:
- Code follows existing style and conventions
- Changes are well-documented
- Testing is performed
- Build system is updated if needed

## Contact

For issues and questions, please refer to the main HIK project documentation.