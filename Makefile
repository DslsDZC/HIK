# HIC 根目录 Makefile
ARCH ?= arm64

# 统一输出目录
O = build

# ARM64: 使用 Makefile.arm64
# x86_64: 使用 build/Makefile（原有）
export ROOT_DIR = $(CURDIR)

ifeq ($(ARCH),arm64)
include Makefile.arm64
else
# x86_64: 原有构建系统
all:
	$(MAKE) -C build

.PHONY: bootloader kernel img
bootloader kernel img:
	$(MAKE) -C build $@

run: all
	$(MAKE) -C build run

clean:
	$(MAKE) -C build clean
endif