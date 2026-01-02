/*
 * HIK BIOS Bootloader - Hardware Abstraction Layer (HAL) Implementation
 */

#include "hal.h"
#include "../util/util.h"

/* VGA cursor position */
static int vga_cursor_x = 0;
static int vga_cursor_y = 0;
static uint8_t vga_color = 0x0F;  /* White on black */

/* Memory map */
static memory_map_entry_t memory_map[MAX_MEMORY_MAP_ENTRIES] __attribute__((unused));
static uint32_t memory_map_count = 0;

/*
 * Initialize HAL
 */
int hal_init(void) {
    /* Initialize serial port */
    serial_init(SERIAL_PORT_COM1);
    
    /* Initialize VGA */
    vga_cursor_x = 0;
    vga_cursor_y = 0;
    vga_color = 0x0F;
    
    /* Clear screen */
    hal_clear_screen();
    
    return 1;
}

/*
 * Clear screen
 */
void hal_clear_screen(void) {
    uint16_t *vga = (uint16_t*)VGA_BUFFER;
    int i;
    
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = (vga_color << 8) | ' ';
    }
    
    vga_cursor_x = 0;
    vga_cursor_y = 0;
}

/*
 * Print string
 */
void hal_print(const char *str) {
    /* Print to VGA */
    vga_puts(str);
    
    /* Print to serial */
    serial_puts(SERIAL_PORT_COM1, str);
}

/*
 * Print hexadecimal number
 */
void hal_print_hex(uint64_t value) {
    char buffer[17];
    int i;
    
    for (i = 15; i >= 0; i--) {
        uint8_t nibble = (value >> (i * 4)) & 0xF;
        buffer[15 - i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    buffer[16] = '\0';
    
    hal_print("0x");
    hal_print(buffer);
}

/*
 * Print decimal number
 */
void hal_print_dec(uint64_t value) {
    char buffer[21];
    int i = 0;
    
    if (value == 0) {
        hal_print("0");
        return;
    }
    
    while (value > 0 && i < 20) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    while (i > 0) {
        vga_putc(buffer[--i]);
    }
}

/*
 * Wait for key press
 */
void hal_wait_key(void) {
    /* Wait for keyboard input */
    while (!(inb(0x64) & 1)) {
        /* Wait */
    }
    inb(0x60);  /* Read key */
}

/*
 * Reboot system
 */
void hal_reboot(void) {
    /* Disable interrupts */
    asm volatile("cli");
    
    /* Reboot via keyboard controller */
    outb(0x64, 0xFE);
    
    /* Halt if reboot fails */
    while (1) {
        asm volatile("hlt");
    }
}

/*
 * Detect memory using BIOS INT 15h, E820
 */
int hal_detect_memory(hik_boot_info_t *boot_info) {
    uint32_t continuation = 0;
    uint32_t entries = 0;
    uint32_t ebx;
    
    memory_map_entry_t *map = (memory_map_entry_t*)MEMORY_MAP_ADDR;
    
    /* Use BIOS INT 15h, E820 to get memory map */
    do {
        uint32_t signature, size;
        
        /* Setup registers for INT 15h, E820 */
        asm volatile(
            "movl $0xE820, %%eax\n"
            "movl $24, %%edx\n"
            "int $0x15\n"
            : "=a"(signature), "=b"(ebx), "=c"(size)
            : "a"(0xE820), "b"(continuation), "c"(24), "d"(0x534D4150)  /* 'SMAP' */
        );

        if (signature != 0x534D4150 || size < 20) {
            break;
        }
        
        /* Copy entry to memory map */
        if (entries < MAX_MEMORY_MAP_ENTRIES) {
            memcpy(&map[entries], (void*)0x5000, sizeof(memory_map_entry_t));
            entries++;
        }
        
        continuation = ebx;
        
    } while (continuation != 0 && entries < MAX_MEMORY_MAP_ENTRIES);
    
    /* Add conventional memory (0 - 640KB) if not present */
    if (entries == 0) {
        map[0].base_address = 0;
        map[0].length = 0xA0000;
        map[0].type = MEMORY_TYPE_USABLE;
        map[0].attributes = 0;
        entries++;
    }
    
    /* Add extended memory if detected */
    const volatile uint16_t *mem_ptr = (const volatile uint16_t*)0x413;
uint16_t ext_mem = *mem_ptr;  /* BIOS data area */
    if (ext_mem > 640) {
        map[entries].base_address = 0x100000;
        map[entries].length = (ext_mem - 640) * 1024;
        map[entries].type = MEMORY_TYPE_USABLE;
        map[entries].attributes = 0;
        entries++;
    }
    
    /* Add reserved memory regions */
    map[entries].base_address = 0;
    map[entries].length = 0x500;
    map[entries].type = MEMORY_TYPE_RESERVED;
    map[entries].attributes = 0;
    entries++;
    
    map[entries].base_address = 0x9FC00;
    map[entries].length = 0x400;
    map[entries].type = MEMORY_TYPE_RESERVED;
    map[entries].attributes = 0;
    entries++;
    
    /* Store memory map info */
    memory_map_count = entries;
    
    /* Update boot info */
    boot_info->memory_map_base = MEMORY_MAP_ADDR;
    boot_info->memory_map_size = entries * sizeof(memory_map_entry_t);
    boot_info->memory_map_desc_size = sizeof(memory_map_entry_t);
    boot_info->memory_map_count = entries;
    
    /* Print memory map */
    hal_print("Memory map:\n");
    for (uint32_t i = 0; i < entries; i++) {
        hal_print("  [");
        hal_print_dec(i);
        hal_print("] Base: 0x");
        hal_print_hex(map[i].base_address);
        hal_print(", Size: ");
        hal_print_dec(map[i].length / 1024);
        hal_print(" KB, Type: ");
        hal_print_dec(map[i].type);
        hal_print("\n");
    }
    
    return 1;
}

/*
 * Get memory map size
 */
uint64_t hal_get_memory_map_size(void) {
    return memory_map_count * sizeof(memory_map_entry_t);
}

/*
 * Get memory map count
 */
uint32_t hal_get_memory_map_count(void) {
    return memory_map_count;
}

/* Forward declaration for static function */
static uint64_t search_rsdp(uint64_t start, uint64_t end);

/*
 * Find ACPI RSDP
 */
uint64_t hal_find_rsdp(void) {
    /* Search for RSDP in EBDA (Extended BIOS Data Area) */
    const volatile uint16_t *ebda_ptr = (const volatile uint16_t*)0x40E;
    uint16_t ebda_base = *ebda_ptr << 4;
    
    if (ebda_base != 0) {
        uint64_t rsdp = search_rsdp(ebda_base, 0xA0000);
        if (rsdp != 0) {
            return rsdp;
        }
    }
    
    /* Search in low memory */
    return search_rsdp(0xE0000, 0xFFFFF);
}

/*
 * Search for RSDP signature in memory range
 */
static uint64_t search_rsdp(uint64_t start, uint64_t end) {
    for (uint64_t addr = start; addr < end; addr += 16) {
        const uint8_t *ptr = (const uint8_t*)(uintptr_t)addr;
        if (memcmp(ptr, "RSD PTR ", 8) == 0) {
            /* Verify checksum */
            uint8_t sum = 0;
            for (int i = 0; i < 20; i++) {
                sum += ptr[i];
            }

            if (sum == 0) {
                return addr;
            }
        }
    }

    return 0;
}

/*
 * Initialize serial port
 */
void serial_init(uint16_t port) {
    outb(port + 1, 0x00);  /* Disable interrupts */
    outb(port + 3, 0x80);  /* Enable DLAB (set baud rate divisor) */
    outb(port + 0, 0x01);  /* Set divisor to 1 (lo byte) 115200 baud */
    outb(port + 1, 0x00);  /*                  (hi byte) */
    outb(port + 3, 0x03);  /* 8 bits, no parity, one stop bit */
    outb(port + 2, 0xC7);  /* Enable FIFO, clear them, 14-byte threshold */
    outb(port + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
}

/*
 * Send character to serial port
 */
void serial_putc(uint16_t port, char c) {
    while ((inb(port + 5) & 0x20) == 0) {
        /* Wait for transmit buffer empty */
    }
    outb(port, c);
}

/*
 * Send string to serial port
 */
void serial_puts(uint16_t port, const char *str) {
    while (*str) {
        serial_putc(port, *str++);
    }
}

/*
 * Put character to VGA
 */
void vga_putc(char c) {
    uint16_t *vga = (uint16_t*)VGA_BUFFER;
    
    switch (c) {
        case '\n':
            vga_cursor_x = 0;
            vga_cursor_y++;
            break;
            
        case '\r':
            vga_cursor_x = 0;
            break;
            
        case '\t':
            vga_cursor_x = (vga_cursor_x + 8) & ~7;
            break;
            
        default:
            if (vga_cursor_x < VGA_WIDTH && vga_cursor_y < VGA_HEIGHT) {
                int offset = vga_cursor_y * VGA_WIDTH + vga_cursor_x;
                vga[offset] = (vga_color << 8) | c;
                vga_cursor_x++;
            }
            break;
    }
    
    /* Scroll if necessary */
    if (vga_cursor_y >= VGA_HEIGHT) {
        /* Scroll up */
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga[i] = vga[i + VGA_WIDTH];
        }
        
        /* Clear last line */
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga[i] = (vga_color << 8) | ' ';
        }
        
        vga_cursor_y = VGA_HEIGHT - 1;
    }
}

/*
 * Put string to VGA
 */
void vga_puts(const char *str) {
    while (*str) {
        vga_putc(*str++);
    }
}

/*
 * Set VGA color
 */
void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = (bg << 4) | (fg & 0x0F);
}

/*
 * Halt system
 */
void halt(void) {
    asm volatile("cli");
    while (1) {
        asm volatile("hlt");
    }
}
