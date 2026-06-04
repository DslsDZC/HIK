#!/usr/bin/env python3
"""
创建FAT32磁盘镜像，包含UEFI引导文件、内核和HIC模块文件（无需sudo）

用法:
    python3 create_efi_disk_no_root.py \
        --bootloader <bootx64.efi> \
        --kernel <hic-kernel.bin> \
        --output <disk.img> \
        [--modules-dir <modules_dir>]

参数:
    --bootloader     UEFI引导程序路径 (BOOTX64.EFI)
    --kernel         内核二进制路径 (hic-kernel.bin)
    --output         输出磁盘镜像路径
    --modules-dir    Privileged-1 模块目录（包含 .hicmod 文件），可选
"""
import argparse
import os
import subprocess
import sys


def create_fat32_image(disk_path, bootloader_path, kernel_path,
                       modules_dir=None):
    """创建FAT32磁盘镜像，包含引导文件、内核和可选的模块文件"""
    print(f"创建FAT32磁盘镜像: {disk_path}")
    print(f"引导程序: {bootloader_path}")
    print(f"内核: {kernel_path}")

    # 检查文件是否存在
    if not os.path.exists(bootloader_path):
        print(f"错误: 引导程序不存在: {bootloader_path}")
        return False

    if not os.path.exists(kernel_path):
        print(f"错误: 内核不存在: {kernel_path}")
        return False

    # 如果内核是 ELF 文件，先转为 .bin
    if kernel_path.endswith('.elf'):
        bin_path = kernel_path.replace('.elf', '.bin')
        if not os.path.exists(bin_path):
            print(f"创建二进制文件: {bin_path}")
            subprocess.run([
                'objcopy', '-O', 'binary', kernel_path, bin_path
            ], check=True)
        kernel_path = bin_path

    try:
        # 计算需要的块数
        bootloader_size = os.path.getsize(bootloader_path)
        kernel_size = os.path.getsize(kernel_path)

        # 计算模块总大小
        modules_size = 0
        if modules_dir and os.path.isdir(modules_dir):
            for f in os.listdir(modules_dir):
                if f.endswith('.hicmod'):
                    modules_size += os.path.getsize(
                        os.path.join(modules_dir, f))

        total_size = bootloader_size + kernel_size + modules_size + 64 * 1024 * 1024  # 额外64MB
        # 确保大小是 1MB 对齐（更好的兼容性）
        total_size = ((total_size + 1024 * 1024 - 1) // (1024 * 1024)) * (1024 * 1024)
        block_count = total_size // 512

        # 创建FAT32镜像
        print(f"创建镜像文件 (块数: {block_count}, 大小: {total_size // 1024 // 1024}MB)...")
        subprocess.run([
            'dd', 'if=/dev/zero', f'of={disk_path}',
            f'bs=512', f'count={block_count}'
        ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        # 格式化为FAT32
        print("格式化为FAT32...")
        subprocess.run([
            'mkfs.vfat', '-F', '32', disk_path, '-n', 'HICBOOT'
        ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        # 使用mtools复制文件 (无需sudo)
        print("复制EFI引导文件...")
        # 创建EFI/BOOT目录
        subprocess.run([
            'mmd', '-i', disk_path, 'EFI'
        ], check=True)
        subprocess.run([
            'mmd', '-i', disk_path, 'EFI/BOOT'
        ], check=True)

        # 复制引导程序
        subprocess.run([
            'mcopy', '-i', disk_path, bootloader_path, '::EFI/BOOT/BOOTX64.EFI'
        ], check=True)

        # 复制内核
        print("复制内核...")
        subprocess.run([
            'mcopy', '-i', disk_path, kernel_path, '::hic-kernel.bin'
        ], check=True)

        # 复制模块文件
        if modules_dir and os.path.isdir(modules_dir):
            print("复制模块文件...")
            subprocess.run([
                'mmd', '-i', disk_path, 'modules'
            ], check=True)

            module_files = sorted([
                f for f in os.listdir(modules_dir) if f.endswith('.hicmod')
            ])
            for f in module_files:
                src = os.path.join(modules_dir, f)
                # FAT32 驱动不支持长文件名，用 8.3 短名：名(≤8).扩展名(≤3)
                base = f.replace('.hicmod', '')[:8].upper()
                ext = 'HIC'  # .hicmod → HIC
                fat_name = f"{base}.{ext}"
                dest = f'::modules/{fat_name}'
                print(f"  复制模块: {f} → {fat_name}")
                subprocess.run([
                    'mcopy', '-i', disk_path, src, dest
                ], check=True)

            print(f"  共复制 {len(module_files)} 个模块文件")

            # 自动生成 MODULES.LIS（排除已在运行的模块管理器）
            modules_list = ""
            for f in module_files:
                if f.startswith('module_manager'): continue
                mod_name = f.replace('.hicmod', '')[:8].upper()
                modules_list += f"{mod_name} auto:yes\n"
            list_tmp = '/tmp/hic_MODULES.LIS'
            with open(list_tmp, 'w') as fh:
                fh.write(modules_list)
            # 同时写入 long 和 short 两种名字，以便驱动兼容
            for dest in ['::modules.list', '::MODULES.LIS']:
                subprocess.run([
                    'mcopy', '-i', disk_path, list_tmp, dest
                ], check=True)
            os.unlink(list_tmp)
            print(f"  已生成 modules.list ({len(module_files)} 个模块)")

        print(f"✓ 磁盘镜像创建成功: {disk_path}")
        return True

    except subprocess.CalledProcessError as e:
        print(f"错误: 创建FAT32镜像失败: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="创建HIC启动磁盘镜像（无需sudo）")
    parser.add_argument('--bootloader', required=True,
                        help='UEFI引导程序路径 (BOOTX64.EFI)')
    parser.add_argument('--kernel', required=True,
                        help='内核二进制路径 (hic-kernel.bin)')
    parser.add_argument('--modules-dir',
                        help='Privileged-1 模块目录（包含 .hicmod 文件）')
    parser.add_argument('--output', required=True,
                        help='输出磁盘镜像路径')
    parser.add_argument('--bootloader-dir',
                        help='引导程序目录（替代 --bootloader）')

    args = parser.parse_args()

    # 兼容旧用法：如果提供了 bootloader-dir，拼接默认文件名
    bootloader = args.bootloader
    if args.bootloader_dir and not bootloader:
        bootloader = os.path.join(args.bootloader_dir, 'BOOTX64.EFI')

    if not create_fat32_image(args.output, bootloader, args.kernel,
                              args.modules_dir):
        sys.exit(1)


if __name__ == '__main__':
    main()
