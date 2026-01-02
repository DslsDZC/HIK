# HIK UEFI Bootloader Build Instructions

## Prerequisites

### Required Tools

- **GCC** (GNU Compiler Collection) - For compiling C code
- **NASM** (Netwide Assembler) - For assembly code (if needed)
- **LD** (GNU Linker) - For linking
- **Objcopy** - For creating EFI files

### Installing on Arch Linux

```bash
sudo pacman -S gcc nasm binutils
```

### Installing on Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install build-essential nasm binutils
```

### Installing on Fedora

```bash
sudo dnf install gcc nasm binutils
```

## Building the Bootloader

### Using the Build Script (Recommended)

The easiest way to build the bootloader is to use the provided build script:

```bash
cd Bootloader/x86_64/uefi
chmod +x tools/build.sh
./tools/build.sh build
```

This will:
1. Check for required tools
2. Create the build directory
3. Compile all source files
4. Link the object files
5. Create the final EFI file

The output will be located at `build/hikboot.efi`.

### Using Make

Alternatively, you can use the Makefile:

```bash
cd Bootloader/x86_64/uefi
make
```

### Clean Build

To clean the build directory:

```bash
./tools/build.sh clean
# or
make clean
```

To rebuild from scratch:

```bash
./tools/build.sh rebuild
# or
make clean && make
```

## Directory Structure

```
Bootloader/x86_64/uefi/
├── efi/           # UEFI protocol and type definitions
├── hal/           # Hardware Abstraction Layer
├── fs/            # File system support
├── security/      # Security and signature verification
├── bootmgr/       # Boot manager core logic
├── util/          # Utility functions
├── tools/         # Build tools and scripts
└── build/         # Build output directory (created during build)
```

## Installation

After building, you need to install the bootloader to your EFI system partition:

1. Create the EFI directory structure:
```bash
sudo mkdir -p /boot/efi/EFI/HIK
```

2. Copy the bootloader:
```bash
sudo cp build/hikboot.efi /boot/efi/EFI/HIK/
```

3. Create a boot configuration file (`/boot/efi/EFI/HIK/boot.conf`):
```
title = "HIK Kernel"
timeout = 5

[entry]
name = "HIK Kernel"
kernel = "\\EFI\\HIK\\kernel.hik"
args = "console=ttyS0,115200"
default = true
```

4. Add the bootloader to your EFI boot entries using `efibootmgr`:
```bash
sudo efibootmgr -c -d /dev/sda -p 1 -L "HIK Boot" -l /EFI/HIK/hikboot.efi
```

## Troubleshooting

### Build Errors

If you encounter build errors, make sure:
- All required tools are installed
- You have write permissions in the build directory
- The source files are not corrupted

### EFI Boot Issues

If the bootloader doesn't boot:
1. Check that the EFI file is correctly placed
2. Verify the boot configuration file format
3. Check the UEFI boot order
4. Enable verbose output in the bootloader for debugging

## Development

### Adding New Features

To add new features to the bootloader:

1. Create source files in the appropriate directory
2. Add the files to the Makefile or build script
3. Rebuild the bootloader
4. Test thoroughly

### Code Style

- Use 4-space indentation
- Follow the existing naming conventions
- Add comments for complex logic
- Keep functions small and focused

## License

This bootloader is part of the HIK (Hierarchical Isolation Kernel) project.

## Support

For issues and questions, please refer to the main HIK project documentation.