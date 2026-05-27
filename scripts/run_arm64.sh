#!/bin/bash
set -e

# HIC ARM64 QEMU 启动脚本
ROOT=$(cd "$(dirname "$0")/.."; pwd)
BUILD=$ROOT/build-arm64
KERNEL=$BUILD/hic-kernel.elf

if [ ! -f "$KERNEL" ]; then
    echo "ERROR: Kernel not found. Run build_arm64.sh first."
    exit 1
fi

# QEMU virt 平台参数:
# - CPU: cortex-a72
# - RAM: 1GB @ 0x40000000
# - UART: PL011 @ 0x09000000
# - GICv3 @ 0x08000000
# - -kernel 加载到 0x40080000

echo "=== Starting HIC on ARM64 QEMU ==="
qemu-system-aarch64 \
    -machine virt,gic-version=3 \
    -cpu cortex-a72 \
    -m 1G \
    -nographic \
    -serial mon:stdio \
    -kernel $KERNEL \
    -append "debug quiet" \
    -d guest_errors \
    -D qemu.log \
    2>&1 | tee $BUILD/qemu-output.log
