# HIC 根目录 Makefile
ARCH ?= x86_64

# 统一输出目录
O = build

export ROOT_DIR = $(CURDIR)

# 按命名约定自动加载架构 Makefile（Makefile.x86_64, Makefile.arm64, ...）
-include Makefile.$(ARCH)
