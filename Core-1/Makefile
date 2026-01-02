# HIK Core-1 Build System

# Compiler and tools
CC = gcc
LD = ld
OBJCOPY = objcopy
AS = gcc -c

# Directories
SRC_DIR = .
INCLUDE_DIR = include
ARCH_DIR = arch/x86_64
MM_DIR = mm
IPC_DIR = ipc
SERVICE_DIR = service
ISOLATION_DIR = isolation
LIB_DIR = lib
BUILD_DIR = build

# Compiler flags
CFLAGS = -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
         -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -O2 \
         -Wno-unused-parameter -Wno-unused-function \
         -I$(INCLUDE_DIR) -I$(SRC_DIR) \
         -m64 -mcmodel=large

ASFLAGS = -x assembler-with-cpp

LDFLAGS = -nostdlib -T $(SRC_DIR)/linker.ld

# Source files
CORE1_SOURCES = core1.c
MM_SOURCES = $(MM_DIR)/physical_mem.c
IPC_SOURCES = $(IPC_DIR)/ipc.c
SERVICE_SOURCES = $(SERVICE_DIR)/service.c
ISOLATION_SOURCES = $(ISOLATION_DIR)/isolation.c
LIB_SOURCES = $(LIB_DIR)/string.c

# All sources
ALL_SOURCES = $(CORE1_SOURCES) $(MM_SOURCES) $(IPC_SOURCES) \
              $(SERVICE_SOURCES) $(ISOLATION_SOURCES) $(LIB_SOURCES)

# Object files
OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(ALL_SOURCES))

# Output files
SERVICE_ELF = $(BUILD_DIR)/service.elf
SERVICE_BIN = $(BUILD_DIR)/service.bin

# Default target
all: directories $(SERVICE_ELF) $(SERVICE_BIN)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR)/mm
	@mkdir -p $(BUILD_DIR)/ipc
	@mkdir -p $(BUILD_DIR)/service
	@mkdir -p $(BUILD_DIR)/isolation
	@mkdir -p $(BUILD_DIR)/lib

# Compile C files
$(BUILD_DIR)/%.o: %.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Link service
$(SERVICE_ELF): $(OBJECTS)
	@echo "Linking service..."
	@$(LD) $(LDFLAGS) -o $@ $^

# Create binary
$(SERVICE_BIN): $(SERVICE_ELF)
	@echo "Creating service binary..."
	@$(OBJCOPY) -O binary $< $@

# Clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Help
help:
	@echo "HIK Core-1 Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all    - Build all components (default)"
	@echo "  clean  - Remove build artifacts"
	@echo "  help   - Show this help message"

.PHONY: all directories clean help