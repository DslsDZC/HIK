#!/bin/bash
# HIK ISO 调试脚本
# 用于调试 ISO 镜像启动过程

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}HIK ISO 调试工具${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 检查依赖
check_dependencies() {
    echo -e "${YELLOW}检查依赖...${NC}"

    if ! command -v qemu-system-x86_64 &> /dev/null; then
        echo -e "${RED}错误: QEMU 未安装${NC}"
        echo "请安装: sudo apt install qemu-system-x86"
        exit 1
    fi
    echo -e "${GREEN}✓ QEMU 已安装${NC}"

    if ! command -v gdb &> /dev/null; then
        echo -e "${YELLOW}警告: GDB 未安装，无法使用调试功能${NC}"
        echo "请安装: sudo apt install gdb"
    else
        echo -e "${GREEN}✓ GDB 已安装${NC}"
    fi

    if ! command -v grub-mkrescue &> /dev/null; then
        echo -e "${YELLOW}警告: grub-mkrescue 未安装${NC}"
        echo "请安装: sudo apt install grub-pc-bin grub-common xorriso"
    else
        echo -e "${GREEN}✓ GRUB 工具已安装${NC}"
    fi

    echo ""
}

# 检查 ISO 文件
check_iso() {
    echo -e "${YELLOW}检查 ISO 文件...${NC}"

    if [ ! -f "build/hik.iso" ]; then
        echo -e "${RED}错误: ISO 文件不存在${NC}"
        echo "请先运行: make iso"
        exit 1
    fi

    ISO_SIZE=$(du -h build/hik.iso | cut -f1)
    echo -e "${GREEN}✓ ISO 文件存在 (${ISO_SIZE})${NC}"

    # 检查 ISO 格式
    if file build/hik.iso | grep -q "ISO 9660"; then
        echo -e "${GREEN}✓ ISO 格式正确${NC}"
    else
        echo -e "${RED}错误: ISO 格式不正确${NC}"
        exit 1
    fi

    echo ""
}

# 显示 ISO 内容
show_iso_contents() {
    echo -e "${YELLOW}ISO 内容:${NC}"
    echo ""

    # 使用 isoinfo 命令（无需 sudo）
    if command -v isoinfo &> /dev/null; then
        echo "文件列表:"
        isoinfo -i build/hik.iso -f -R 2>/dev/null | sed 's/^/  /' || echo "  无法读取 ISO 内容"
    else
        echo -e "${YELLOW}提示: 安装 genisoimage 以使用 isoinfo${NC}"
        echo "  sudo apt install genisoimage"
    fi

    echo ""
}

# 显示 GRUB 配置
show_grub_config() {
    echo -e "${YELLOW}GRUB 配置:${NC}"
    echo ""

    if command -v isoinfo &> /dev/null; then
        isoinfo -i build/hik.iso -x /boot/grub/grub.cfg 2>/dev/null | sed 's/^/  /' || echo -e "${RED}未找到 GRUB 配置文件${NC}"
    else
        echo -e "${YELLOW}提示: 安装 genisoimage 以读取 GRUB 配置${NC}"
        echo "  sudo apt install genisoimage"
    fi

    echo ""
}

# 检查内核文件
check_kernel() {
    echo -e "${YELLOW}内核文件检查:${NC}"
    echo ""

    if command -v isoinfo &> /dev/null; then
        # 提取内核文件到临时目录
        TEMP_DIR=$(mktemp -d)
        isoinfo -i build/hik.iso -x /boot/kernel.elf > "$TEMP_DIR/kernel.elf" 2>/dev/null || {
            echo -e "${RED}✗ 内核文件不存在${NC}"
            rm -rf "$TEMP_DIR"
            return
        }

        KERNEL_SIZE=$(du -h "$TEMP_DIR/kernel.elf" | cut -f1)
        echo -e "${GREEN}✓ 内核 ELF 文件存在 (${KERNEL_SIZE})${NC}"

        # 检查 ELF 格式
        if file "$TEMP_DIR/kernel.elf" | grep -q "ELF 64-bit"; then
            echo -e "${GREEN}✓ 内核是 64 位 ELF${NC}"
        else
            echo -e "${RED}✗ 内核格式错误${NC}"
        fi

        # 检查 Multiboot2 头
        if hexdump -C "$TEMP_DIR/kernel.elf" | head -1 | grep -q "d6 50 52 e8"; then
            echo -e "${GREEN}✓ Multiboot2 头存在${NC}"
        else
            echo -e "${RED}✗ Multiboot2 头缺失${NC}"
        fi

        rm -rf "$TEMP_DIR"
    else
        echo -e "${YELLOW}提示: 安装 genisoimage 以检查内核文件${NC}"
        echo "  sudo apt install genisoimage"
    fi

    echo ""
}

# 启动调试会话
start_debug() {
    echo -e "${YELLOW}启动调试会话...${NC}"
    echo ""

    if ! command -v gdb &> /dev/null; then
        echo -e "${RED}错误: GDB 未安装${NC}"
        exit 1
    fi

    echo -e "${GREEN}QEMU 将在后台启动，等待 GDB 连接...${NC}"
    echo ""
    echo -e "${BLUE}GDB 命令:${NC}"
    echo "  gdb Core-0/build/kernel.elf"
    echo "  (gdb) target remote :1234"
    echo "  (gdb) break kernel_init"
    echo "  (gdb) continue"
    echo ""
    echo -e "${YELLOW}按 Ctrl+C 停止 QEMU${NC}"
    echo ""

    # 启动 QEMU
    qemu-system-x86_64 \
        -cdrom build/hik.iso \
        -m 512M \
        -serial mon:stdio \
        -S -s \
        -d int,cpu_reset,guest_errors \
        -D /tmp/qemu-debug.log &

    QEMU_PID=$!
    echo -e "${GREEN}QEMU PID: $QEMU_PID${NC}"
    echo -e "${GREEN}调试日志: /tmp/qemu-debug.log${NC}"
    echo ""

    # 等待用户中断
    trap "kill $QEMU_PID 2>/dev/null; echo -e '${YELLOW}QEMU 已停止${NC}'; exit 0" INT

    # 监控日志
    tail -f /tmp/qemu-debug.log 2>/dev/null || true

    wait $QEMU_PID 2>/dev/null || true
}

# 主菜单
show_menu() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}调试选项:${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo "1) 检查 ISO 文件"
    echo "2) 显示 ISO 内容"
    echo "3) 显示 GRUB 配置"
    echo "4) 检查内核文件"
    echo "5) 启动调试会话 (GDB)"
    echo "6) 运行 ISO (无调试)"
    echo "7) 查看调试日志"
    echo "8) 全部检查"
    echo "0) 退出"
    echo ""
}

# 查看调试日志
view_logs() {
    echo -e "${YELLOW}调试日志:${NC}"
    echo ""

    if [ -f "/tmp/qemu-debug.log" ]; then
        tail -50 /tmp/qemu-debug.log
    else
        echo -e "${YELLOW}未找到调试日志${NC}"
    fi

    echo ""
}

# 运行 ISO
run_iso() {
    echo -e "${YELLOW}运行 ISO 镜像...${NC}"
    echo ""
    echo -e "${YELLOW}按 Ctrl+A 然后 X 退出${NC}"
    echo ""

    qemu-system-x86_64 \
        -cdrom build/hik.iso \
        -m 512M \
        -serial mon:stdio
}

# 全部检查
run_all_checks() {
    check_iso
    show_iso_contents
    show_grub_config
    check_kernel
}

# 主程序
main() {
    check_dependencies

    if [ $# -eq 0 ]; then
        # 交互模式
        while true; do
            show_menu
            read -p "请选择 [0-8]: " choice

            case $choice in
                1) check_iso ;;
                2) show_iso_contents ;;
                3) show_grub_config ;;
                4) check_kernel ;;
                5) start_debug ;;
                6) run_iso ;;
                7) view_logs ;;
                8) run_all_checks ;;
                0) echo "退出"; exit 0 ;;
                *) echo -e "${RED}无效选择${NC}" ;;
            esac

            echo ""
            read -p "按 Enter 继续..."
            clear
        done
    else
        # 命令行模式
        case $1 in
            check) check_iso ;;
            contents) show_iso_contents ;;
            grub) show_grub_config ;;
            kernel) check_kernel ;;
            debug) start_debug ;;
            run) run_iso ;;
            logs) view_logs ;;
            all) run_all_checks ;;
            *) echo "用法: $0 [check|contents|grub|kernel|debug|run|logs|all]" ;;
        esac
    fi
}

main "$@"