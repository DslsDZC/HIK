# HIK Core-0 Build System

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
SCHED_DIR = sched
CAPABILITY_DIR = capability
SERVICE_DIR = service
PROCESS_DIR = process
STARTUP_DIR = startup
IRQ_DIR = irq
ISOLATION_DIR = isolation
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
ARCH_SOURCES = $(ARCH_DIR)/longmode.S
MM_SOURCES = $(MM_DIR)/mm.c
SCHED_SOURCES = $(SCHED_DIR)/sched.c
CAPABILITY_SOURCES = $(CAPABILITY_DIR)/capability.c
SERVICE_SOURCES = $(SERVICE_DIR)/service.c
PROCESS_SOURCES = $(PROCESS_DIR)/process.c
STARTUP_SOURCES = $(STARTUP_DIR)/kernel.c $(STARTUP_DIR)/multiboot.S
IRQ_SOURCES = $(IRQ_DIR)/irq.c
ISOLATION_SOURCES = $(ISOLATION_DIR)/isolation.c
MMU_TEST_SOURCES = mmu_test.c
LIB_SOURCES = lib/string.c lib/debug.c

# All sources
ALL_SOURCES = $(ARCH_SOURCES) $(MM_SOURCES) $(SCHED_SOURCES) \
              $(CAPABILITY_SOURCES) $(SERVICE_SOURCES) $(PROCESS_SOURCES) \
              $(STARTUP_SOURCES) $(IRQ_SOURCES) $(ISOLATION_SOURCES) \
              $(MMU_TEST_SOURCES) $(LIB_SOURCES)

# Object files
OBJECTS = $(patsubst %.S,$(BUILD_DIR)/%.o,$(ARCH_SOURCES)) \
          $(patsubst %.c,$(BUILD_DIR)/%.o,$(MM_SOURCES)) \
          $(patsubst %.c,$(BUILD_DIR)/%.o,$(SCHED_SOURCES)) \
          $(patsubst %.c,$(BUILD_DIR)/%.o,$(CAPABILITY_SOURCES)) \
          $(patsubst %.c,$(BUILD_DIR)/%.o,$(SERVICE_SOURCES)) \
          $(patsubst %.c,$(BUILD_DIR)/%.o,$(PROCESS_SOURCES)) \
          $(filter %.o,$(patsubst %.S,$(BUILD_DIR)/%.o,$(STARTUP_SOURCES)) $(patsubst %.c,$(BUILD_DIR)/%.o,$(STARTUP_SOURCES))) \
          $(patsubst %.c,$(BUILD_DIR)/%.o,$(IRQ_SOURCES)) \
          $(patsubst %.c,$(BUILD_DIR)/%.o,$(ISOLATION_SOURCES)) \
          $(patsubst %.c,$(BUILD_DIR)/%.o,$(MMU_TEST_SOURCES)) \
          $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIB_SOURCES))

# Output files
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin

# Default target
all: directories $(KERNEL_ELF) $(KERNEL_BIN)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR)/arch/x86_64
	@mkdir -p $(BUILD_DIR)/mm
	@mkdir -p $(BUILD_DIR)/sched
	@mkdir -p $(BUILD_DIR)/capability
	@mkdir -p $(BUILD_DIR)/service
	@mkdir -p $(BUILD_DIR)/process
	@mkdir -p $(BUILD_DIR)/startup
	@mkdir -p $(BUILD_DIR)/irq
	@mkdir -p $(BUILD_DIR)/isolation
	@mkdir -p $(BUILD_DIR)/lib

# Compile C files
$(BUILD_DIR)/%.o: %.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Assemble assembly files
$(BUILD_DIR)/%.o: %.S
	@echo "Assembling $<..."
	@$(AS) $(ASFLAGS) $< -o $@

# Link kernel
$(KERNEL_ELF): $(OBJECTS)
	@echo "Linking kernel..."
	@$(LD) $(LDFLAGS) -o $@ $(BUILD_DIR)/startup/multiboot.o $(filter-out $(BUILD_DIR)/startup/multiboot.o,$^)

# Create binary
$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "Creating kernel binary..."
	@$(OBJCOPY) -O binary $< $@

# Clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Help
help:
	@echo "HIK Core-0 Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all    - Build all components (default)"
	@echo "  clean  - Remove build artifacts"
	@echo "  help   - Show this help message"

.PHONY: all directories clean help