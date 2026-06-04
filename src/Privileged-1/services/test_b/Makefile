SERVICE_NAME := test_b
SERVICE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -ffreestanding -nostdinc -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -mno-3dnow
CFLAGS += -I.
OBJS := $(OBJ_DIR)/$(SERVICE_NAME).service.o
TARGET := $(OBJ_DIR)/$(SERVICE_NAME).o
MOD_TARGET := $(MOD_DIR)/$(SERVICE_NAME).hicmod
TXT_FILE := hicmod.txt
.PHONY: all clean module
all: $(TARGET)
module: $(MOD_TARGET)
$(MOD_TARGET): $(TARGET) $(TXT_FILE)
	@python3 $(ROOT_DIR)/scripts/create_hicmod.py $(TARGET) $(TXT_FILE) $(MOD_TARGET)
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -r $^ -o $@
$(OBJ_DIR)/$(SERVICE_NAME).service.o: service.c
	$(CC) $(CFLAGS) -c $< -o $@
clean: ; rm -f $(OBJS) $(TARGET) $(MOD_TARGET)
