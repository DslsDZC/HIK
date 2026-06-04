# HIC 根目录 Makefile
ARCH ?= x86_64

# 统一输出目录
O = build

export ROOT_DIR = $(CURDIR)

ifeq ($(ARCH),x86_64)
include Makefile.x86_64
else ifeq ($(ARCH),arm64)
include Makefile.arm64
else ifeq ($(ARCH),stm32f103)
include Makefile.stm32f103
endif
