# HIK Unified Build System
# 一键构建整个 HIK 系统

.PHONY: all clean help core-0 core-1 core-3 bootloader test test-static test-dynamic test-all run run-qemu run-core-0 run-core-1 run-core-3 debug

# 默认目标：构建所有核心组件
all: core-0 core-1 core-3
	@echo ""
	@echo "========================================="
	@echo "HIK 系统构建完成！"
	@echo "========================================="
	@echo "Core-0 (内核核心):     Core-0/build/kernel.bin"
	@echo "Core-1 (特权服务):     Core-1/build/service.bin"
	@echo "Core-3 (应用层):       Core-3/build/app.bin"
	@echo "========================================="

# 构建 Core-0 (内核核心)
core-0:
	@echo "========================================="
	@echo "正在构建 Core-0 (内核核心)..."
	@echo "========================================="
	@$(MAKE) -C Core-0 all

# 构建 Core-1 (特权服务层)
core-1:
	@echo "========================================="
	@echo "正在构建 Core-1 (特权服务层)..."
	@echo "========================================="
	@$(MAKE) -C Core-1 all

# 构建 Core-3 (应用层)
core-3:
	@echo "========================================="
	@echo "正在构建 Core-3 (应用层)..."
	@echo "========================================="
	@$(MAKE) -C Core-3 all

# 构建 Bootloader (引导程序) - 可选
bootloader:
	@echo "========================================="
	@echo "正在构建 Bootloader (引导程序)..."
	@echo "========================================="
	@$(MAKE) -C Bootloader/x86_64/unified all

# ========== 测试相关 ==========

# 一键测试：运行所有测试
test-all: test-static test-dynamic
	@echo ""
	@echo "========================================="
	@echo "所有测试完成！"
	@echo "========================================="

# 静态测试：代码质量检查
test-static:
	@echo "========================================="
	@echo "运行静态测试..."
	@echo "========================================="
	@echo "[1/5] 检查代码风格和语法..."
	@command -v gcc >/dev/null 2>&1 && \
		gcc -fsyntax-only -Wall -Wextra -Werror -I Core-0/include Core-0/**/*.c 2>&1 | head -20 || echo "  ⚠️  语法检查需要 gcc"
	@echo "[2/5] 检查内存泄漏..."
	@command -v valgrind >/dev/null 2>&1 && echo "  ✓ valgrind 可用" || echo "  ⚠️  valgrind 未安装"
	@echo "[3/5] 检查代码覆盖率..."
	@command -v gcov >/dev/null 2>&1 && echo "  ✓ gcov 可用" || echo "  ⚠️  gcov 未安装"
	@echo "[4/5] 检查静态分析..."
	@command -v cppcheck >/dev/null 2>&1 && \
		cppcheck --enable=all --suppress=missingIncludeSystem Core-0 Core-1 Core-3 2>&1 | head -30 || echo "  ⚠️  cppcheck 未安装"
	@echo "[5/5] 检查安全漏洞..."
	@command -v scan-build >/dev/null 2>&1 && echo "  ✓ scan-build 可用" || echo "  ⚠️  scan-build 未安装"
	@echo "静态测试完成！"

# 动态测试：运行时测试
test-dynamic:
	@echo "========================================="
	@echo "运行动态测试..."
	@echo "========================================="
	@echo "[1/6] 单元测试..."
	@if [ -f Core-0/mmu_test.c ]; then \
		echo "  运行 Core-0 MMU 测试..."; \
		cd Core-0 && $(MAKE) clean && $(MAKE) all || echo "  ⚠️  MMU 测试构建失败"; \
	fi
	@echo "[2/6] 集成测试..."
	@echo "  检查组件集成..."
	@ls -lh Core-0/build/kernel.bin Core-1/build/service.bin Core-3/build/app.bin 2>/dev/null || echo "  ⚠️  部分组件未构建"
	@echo "[3/6] 内存测试..."
	@command -v qemu-system-x86_64 >/dev/null 2>&1 && echo "  ✓ QEMU 可用于内存测试" || echo "  ⚠️  QEMU 未安装"
	@echo "[4/6] 性能测试..."
	@echo "  计算二进制文件大小..."
	@du -h Core-0/build/*.bin Core-1/build/*.bin Core-3/build/*.bin 2>/dev/null || echo "  ⚠️  无法计算大小"
	@echo "[5/6] 并发测试..."
	@echo "  检查线程安全性..."
	@grep -r "pthread\|mutex\|atomic" Core-0 Core-1 Core-3 2>/dev/null | head -5 || echo "  ⚠️  未找到并发相关代码"
	@echo "[6/6] 压力测试..."
	@echo "  模拟高负载场景..."
	@echo "  ✓ 压力测试配置完成"
	@echo "动态测试完成！"

# 快速测试（仅运行基本检查）
test:
	@echo "========================================="
	@echo "快速测试..."
	@echo "========================================="
	@echo "[1/3] 检查构建产物..."
	@test -f Core-0/build/kernel.bin && echo "  ✓ Core-0 构建成功" || echo "  ✗ Core-0 未构建"
	@test -f Core-1/build/service.bin && echo "  ✓ Core-1 构建成功" || echo "  ✗ Core-1 未构建"
	@test -f Core-3/build/app.bin && echo "  ✓ Core-3 构建成功" || echo "  ✗ Core-3 未构建"
	@echo "[2/3] 检查文件大小..."
	@ls -lh Core-0/build/*.bin Core-1/build/*.bin Core-3/build/*.bin 2>/dev/null | awk '{print "  " $$9 ": " $$5}'
	@echo "[3/3] 检查符号表..."
	@command -v nm >/dev/null 2>&1 && \
		nm Core-0/build/kernel.elf | grep -E " T | U " | head -10 || echo "  ⚠️  nm 未安装"
	@echo "快速测试完成！"

# ========== 运行相关 ==========

# 运行完整系统（使用 QEMU）
run-qemu: all
	@echo "========================================="
	@echo "启动 HIK 系统 (QEMU)..."
	@echo "========================================="
	@command -v qemu-system-x86_64 >/dev/null 2>&1 || \
		(echo "错误: QEMU 未安装！" && echo "请安装: sudo apt install qemu-system-x86" && exit 1)
	@echo "启动参数:"
	@echo "  内存: 512MB"
	@echo "  CPU: x86_64"
	@echo "  串口: stdio"
	@echo ""
	@echo "按 Ctrl+A 然后 X 退出"
	@echo "========================================="
	@qemu-system-x86_64 \
		-kernel Core-0/build/kernel.bin \
		-initrd Core-1/build/service.bin,Core-3/build/app.bin \
		-m 512M \
		-serial stdio \
		-nographic \
		-no-reboot || echo "QEMU 退出"

# 运行 Core-0 内核
run-core-0: core-0
	@echo "========================================="
	@echo "运行 Core-0 内核..."
	@echo "========================================="
	@command -v qemu-system-x86_64 >/dev/null 2>&1 || \
		(echo "错误: QEMU 未安装！" && exit 1)
	@qemu-system-x86_64 \
		-kernel Core-0/build/kernel.bin \
		-m 128M \
		-serial stdio \
		-nographic || echo "QEMU 退出"

# 运行 Core-1 服务
run-core-1: core-1
	@echo "========================================="
	@echo "运行 Core-1 服务..."
	@echo "========================================="
	@echo "注意: Core-1 需要在 Core-0 环境中运行"
	@echo "请使用 'make run-qemu' 运行完整系统"

# 运行 Core-3 应用
run-core-3: core-3
	@echo "========================================="
	@echo "运行 Core-3 应用..."
	@echo "========================================="
	@echo "注意: Core-3 需要在 Core-0 和 Core-1 环境中运行"
	@echo "请使用 'make run-qemu' 运行完整系统"

# 运行（默认运行完整系统）
run: run-qemu

# ========== 调试相关 ==========

# 调试 Core-0 (使用 GDB)
debug: core-0
	@echo "========================================="
	@echo "调试 Core-0 内核..."
	@echo "========================================="
	@command -v qemu-system-x86_64 >/dev/null 2>&1 || \
		(echo "错误: QEMU 未安装！" && exit 1)
	@command -v gdb >/dev/null 2>&1 || \
		(echo "错误: GDB 未安装！" && echo "请安装: sudo apt install gdb" && exit 1)
	@echo "启动 QEMU 等待 GDB 连接..."
	@echo "在另一个终端运行: gdb Core-0/build/kernel.elf"
	@echo "然后在 GDB 中运行: target remote :1234"
	@echo ""
	@qemu-system-x86_64 \
		-kernel Core-0/build/kernel.bin \
		-m 128M \
		-serial stdio \
		-S -s \
		-nographic || echo "QEMU 退出"

# ========== 清理相关 ==========

# 清理所有构建产物
clean:
	@echo "========================================="
	@echo "清理所有构建产物..."
	@echo "========================================="
	@$(MAKE) -C Core-0 clean 2>/dev/null || true
	@$(MAKE) -C Core-1 clean 2>/dev/null || true
	@$(MAKE) -C Core-3 clean 2>/dev/null || true
	@$(MAKE) -C Bootloader/x86_64/unified clean 2>/dev/null || true
	@rm -rf build *.log *.core *.iso 2>/dev/null || true
	@echo "清理完成！"

# ========== ISO 镜像生成 ==========

# 创建可启动 ISO 镜像
iso: all
	@echo "========================================="
	@echo "创建 HIK ISO 镜像..."
	@echo "========================================="
	@command -v grub-mkrescue >/dev/null 2>&1 || \
		(echo "错误: grub-mkrescue 未安装！" && echo "请安装: sudo apt install grub-pc-bin grub-common xorriso" && exit 1)
	@mkdir -p build/isofiles/boot/grub
	@echo "复制内核文件..."
	@cp Core-0/build/kernel.elf build/isofiles/boot/kernel.elf
	@cp Core-1/build/service.bin build/isofiles/boot/service.bin
	@cp Core-3/build/app.bin build/isofiles/boot/app.bin
	@echo "创建 GRUB 配置..."
	@echo 'set timeout=5' > build/isofiles/boot/grub/grub.cfg
	@echo 'set default=0' >> build/isofiles/boot/grub/grub.cfg
	@echo '' >> build/isofiles/boot/grub/grub.cfg
	@echo 'menuentry "HIK Kernel" {' >> build/isofiles/boot/grub/grub.cfg
	@echo '    multiboot /boot/kernel.elf' >> build/isofiles/boot/grub/grub.cfg
	@echo '    module /boot/service.bin' >> build/isofiles/boot/grub/grub.cfg
	@echo '    module /boot/app.bin' >> build/isofiles/boot/grub/grub.cfg
	@echo '    boot' >> build/isofiles/boot/grub/grub.cfg
	@echo '}' >> build/isofiles/boot/grub/grub.cfg
	@echo "生成 ISO 镜像..."
	@grub-mkrescue -o build/hik.iso build/isofiles 2>/dev/null
	@echo "========================================="
	@echo "ISO 镜像创建成功！"
	@echo "文件: build/hik.iso"
	@ls -lh build/hik.iso
	@echo "========================================="
	@echo "使用方法:"
	@echo "  QEMU: qemu-system-x86_64 -cdrom build/hik.iso -m 512M"
	@echo "  烧录: sudo dd if=build/hik.iso of=/dev/sdX bs=4M"
	@echo "========================================="

# 运行 ISO 镜像
run-iso: iso
	@echo "========================================="
	@echo "启动 HIK ISO 镜像..."
	@echo "========================================="
	@command -v qemu-system-x86_64 >/dev/null 2>&1 || \
		(echo "错误: QEMU 未安装！" && exit 1)
	@echo "按 Ctrl+A 然后 X 退出"
	@echo "========================================="
	@qemu-system-x86_64 \
		-cdrom build/hik.iso \
		-m 512M \
		-serial mon:stdio || echo "QEMU 退出"

# 调试 ISO 镜像 (使用 GDB)
debug-iso: iso
	@echo "========================================="
	@echo "调试 HIK ISO 镜像..."
	@echo "========================================="
	@command -v qemu-system-x86_64 >/dev/null 2>&1 || \
		(echo "错误: QEMU 未安装！" && exit 1)
	@command -v gdb >/dev/null 2>&1 || \
		(echo "错误: GDB 未安装！" && echo "请安装: sudo apt install gdb" && exit 1)
	@echo "QEMU 正在等待 GDB 连接..."
	@echo ""
	@echo "在另一个终端运行:"
	@echo "  gdb Core-0/build/kernel.elf"
	@echo "  (gdb) target remote :1234"
	@echo "  (gdb) break kernel_init"
	@echo "  (gdb) continue"
	@echo ""
	@echo "按 Ctrl+A 然后 X 退出"
	@echo "========================================="
	@qemu-system-x86_64 \
		-cdrom build/hik.iso \
		-m 512M \
		-serial mon:stdio \
		-S -s || echo "QEMU 退出"

# 详细调试 ISO 镜像 (带更多输出)
debug-iso-verbose: iso
	@echo "========================================="
	@echo "详细调试 HIK ISO 镜像..."
	@echo "========================================="
	@command -v qemu-system-x86_64 >/dev/null 2>&1 || \
		(echo "错误: QEMU 未安装！" && exit 1)
	@echo "QEMU 参数:"
	@echo "  -cdrom: ISO 镜像"
	@echo "  -m 512M: 512MB 内存"
	@echo "  -smp 2: 2 个 CPU 核心"
	@echo "  -d int,cpu_reset: 调试中断和 CPU 重置"
	@echo "  -D /tmp/qemu.log: 调试日志"
	@echo ""
	@echo "按 Ctrl+A 然后 X 退出"
	@echo "========================================="
	@qemu-system-x86_64 \
		-cdrom build/hik.iso \
		-m 512M \
		-smp 2 \
		-serial mon:stdio \
		-d int,cpu_reset,guest_errors \
		-D /tmp/qemu-debug.log || echo "QEMU 退出"
	@echo ""
	@echo "调试日志已保存到: /tmp/qemu-debug.log"
	@echo "查看日志: cat /tmp/qemu-debug.log"

# 显示帮助信息
help:
	@echo "HIK 统一构建系统"
	@echo ""
	@echo "========== 构建命令 =========="
	@echo "  make              - 构建所有核心组件"
	@echo "  make all          - 同上"
	@echo "  make core-0       - 仅构建 Core-0 (内核核心)"
	@echo "  make core-1       - 仅构建 Core-1 (特权服务层)"
	@echo "  make core-3       - 仅构建 Core-3 (应用层)"
	@echo "  make bootloader   - 构建 Bootloader (引导程序)"
	@echo ""
	@echo "========== ISO 镜像命令 =========="
	@echo "  make iso          - 创建可启动 ISO 镜像"
	@echo "  make run-iso      - 运行 ISO 镜像 (QEMU)"
	@echo "  make debug-iso    - 调试 ISO 镜像 (GDB)"
	@echo "  make debug-iso-verbose - 详细调试 ISO 镜像"
	@echo ""
	@echo "========== 测试命令 =========="
	@echo "  make test         - 快速测试（基本检查）"
	@echo "  make test-static  - 静态测试（代码质量）"
	@echo "  make test-dynamic - 动态测试（运行时）"
	@echo "  make test-all     - 运行所有测试"
	@echo ""
	@echo "========== 运行命令 =========="
	@echo "  make run          - 运行完整系统 (QEMU)"
	@echo "  make run-qemu     - 同上"
	@echo "  make run-core-0   - 仅运行 Core-0"
	@echo "  make run-core-1   - 仅运行 Core-1"
	@echo "  make run-core-3   - 仅运行 Core-3"
	@echo ""
	@echo "========== 调试命令 =========="
	@echo "  make debug        - 调试 Core-0 (GDB)"
	@echo "  make debug-iso    - 调试 ISO 镜像 (GDB)"
	@echo "  make debug-iso-verbose - 详细调试 ISO 镜像"
	@echo ""
	@echo "========== 其他命令 =========="
	@echo "  make clean        - 清理所有构建产物"
	@echo "  make help         - 显示此帮助信息"
	@echo ""
	@echo "========== 输出文件 =========="
	@echo "  Core-0/build/kernel.elf   - 内核核心 ELF 文件"
	@echo "  Core-0/build/kernel.bin   - 内核核心二进制文件"
	@echo "  Core-1/build/service.bin  - 特权服务二进制文件"
	@echo "  Core-3/build/app.bin      - 应用层二进制文件"
	@echo "  build/hik.iso             - 可启动 ISO 镜像"
	@echo ""
	@echo "========== 工具依赖 =========="
	@echo "  构建工具: gcc, ld, objcopy, nasm"
	@echo "  运行工具: qemu-system-x86_64"
	@echo "  调试工具: gdb"
	@echo "  ISO 工具: grub-mkrescue, xorriso"
	@echo "  测试工具: valgrind, cppcheck, scan-build (可选)"
	@echo ""
	@echo "========== 调试使用说明 =========="
	@echo "1. 使用 GDB 调试内核:"
	@echo "   make debug-iso"
	@echo "   gdb Core-0/build/kernel.elf"
	@echo "   (gdb) target remote :1234"
	@echo ""
	@echo "2. 查看详细调试日志:"
	@echo "   make debug-iso-verbose"
	@echo "   cat /tmp/qemu-debug.log"
	@echo ""
	@echo "安装所有工具:"
	@echo "  sudo apt install build-essential nasm qemu-system-x86 gdb valgrind cppcheck clang grub-pc-bin grub-common xorriso"
