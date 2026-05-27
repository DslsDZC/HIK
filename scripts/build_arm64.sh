#!/bin/bash
set -e

# HIC ARM64 QEMU 快速构建脚本
ROOT=$(cd "$(dirname "$0")/.."; pwd)
CORE0=$ROOT/src/Core-0
BUILD=$ROOT/build-arm64
mkdir -p $BUILD

# 编译标志
CFLAGS="-ffreestanding -nostdlib -fno-stack-protector -fno-builtin \
        -fno-pic -fno-pie -mno-outline-atomics \
        -Wall -Wextra -Wpedantic -Wno-unused-parameter \
        -I$CORE0 -I$CORE0/include -I$CORE0/lib -I$CORE0/arch/arm64 \
        -DCONFIG_MMU=1 -DCONFIG_DEBUG=1"

# 编译所有 Core-0 源文件（排除 x86_64 特定的）
echo "=== Compiling Core-0 ==="
SRCS=$(find $CORE0 -maxdepth 1 -name "*.c" ! -name "*.c")
# 手动列出需要编译的文件
cd $CORE0
OBJS=""
for f in main.c boot_info.c kernel_start.c hal.c pmm.c capability.c domain.c \
         domain_switch.c scheduler.c thread.c irq.c ipc3.c audit.c \
         amp.c logical_core.c exec_flow.c module_loader.c static_module.c \
         module_primitives.c runtime_config.c minimal_uart.c hardware_probe.c \
         apm.c yaml.c build_config.c formal_verification.c \
         lib/string.c lib/mem.c lib/console.c \
         arch/arm64/hal_impl.c arch/arm64/pagetable.c arch/arm64/gic.c \
         arch/arm64/timer.c arch/arm64/amp.c; do
    echo "  CC $f"
    gcc $CFLAGS -c $f -o $BUILD/$(basename $f .c).o
    OBJS="$OBJS $BUILD/$(basename $f .c).o"
done

# 编译汇编文件
echo "=== Assembling ==="
for f in arch/arm64/entry.S arch/arm64/context.S arch/arm64/interrupts.S; do
    echo "  AS $f"
    gcc $CFLAGS -c $f -o $BUILD/$(basename $f .S).o
    OBJS="$OBJS $BUILD/$(basename $f .S).o"
done

# 链接
echo "=== Linking ==="
gcc -nostdlib -Wl,-T,$CORE0/arch/arm64/linker.ld \
    -Wl,-Map,$BUILD/hic-kernel.map \
    $OBJS -o $BUILD/hic-kernel.elf

# 生成原始二进制
aarch64-linux-gnu-objcopy -O binary $BUILD/hic-kernel.elf $BUILD/hic-kernel.bin

echo "=== Build complete ==="
ls -lh $BUILD/hic-kernel.elf $BUILD/hic-kernel.bin
