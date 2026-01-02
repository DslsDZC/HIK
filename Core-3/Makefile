# HIK Core-3 Build System

# Compiler and tools
CC = gcc
LD = ld
OBJCOPY = objcopy

# Directories
SRC_DIR = .
INCLUDE_DIR = include
MM_DIR = mm
IPC_DIR = ipc
LIB_DIR = lib
BUILD_DIR = build

# Compiler flags
CFLAGS = -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
         -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -O2 \
         -Wno-unused-parameter -Wno-unused-function \
         -I$(INCLUDE_DIR) -I$(SRC_DIR) \
         -m64 -mcmodel=large

LDFLAGS = -nostdlib -T $(SRC_DIR)/linker.ld

# Source files
CORE3_SOURCES = core3.c
MM_SOURCES = $(MM_DIR)/virtual_mem.c
IPC_SOURCES = $(IPC_DIR)/ipc.c
LIB_SOURCES = $(LIB_DIR)/string.c

# All sources
ALL_SOURCES = $(CORE3_SOURCES) $(MM_SOURCES) $(IPC_SOURCES) $(LIB_SOURCES)

# Object files
OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(ALL_SOURCES))

# Output files
APP_ELF = $(BUILD_DIR)/app.elf
APP_BIN = $(BUILD_DIR)/app.bin

# Default target
all: directories $(APP_ELF) $(APP_BIN)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR)/mm
	@mkdir -p $(BUILD_DIR)/ipc
	@mkdir -p $(BUILD_DIR)/lib

# Compile C files
$(BUILD_DIR)/%.o: %.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Link application
$(APP_ELF): $(OBJECTS)
	@echo "Linking application..."
	@$(LD) $(LDFLAGS) -o $@ $^

# Create binary
$(APP_BIN): $(APP_ELF)
	@echo "Creating application binary..."
	@$(OBJCOPY) -O binary $< $@

# Clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Help
help:
	@echo "HIK Core-3 Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all    - Build all components (default)"
	@echo "  clean  - Remove build artifacts"
	@echo "  help   - Show this help message"

.PHONY: all directories clean help