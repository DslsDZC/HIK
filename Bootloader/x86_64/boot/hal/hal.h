/*
 * HIK BIOS Bootloader - Hardware Abstraction Layer (HAL)
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include "../stage2/stage2.h"

/* VGA text mode */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_BUFFER 0xB8000

/* VGA colors */
#define VGA_COLOR_BLACK   0
#define VGA_COLOR_BLUE    1
#define VGA_COLOR_GREEN   2
#define VGA_COLOR_CYAN    3
#define VGA_COLOR_RED     4
#define VGA_COLOR_MAGENTA 5
#define VGA_COLOR_BROWN   6
#define VGA_COLOR_WHITE   7

/* Serial port */
#define SERIAL_PORT_COM1 0x3F8
#define SERIAL_PORT_COM2 0x2F8

/* BIOS data area */
#define BDA_MEMORY_SIZE 0x413

/* Initialize HAL */
int hal_init(void);

/* Clear screen */
void hal_clear_screen(void);

/* Print string */
void hal_print(const char *str);

/* Print hexadecimal number */
void hal_print_hex(uint64_t value);

/* Print decimal number */
void hal_print_dec(uint64_t value);

/* Wait for key press */
void hal_wait_key(void);

/* Reboot system */
void hal_reboot(void);

/* Detect memory */
int hal_detect_memory(hik_boot_info_t *boot_info);

/* Get memory map size */
uint64_t hal_get_memory_map_size(void);

/* Get memory map count */
uint32_t hal_get_memory_map_count(void);

/* Find ACPI RSDP */
uint64_t hal_find_rsdp(void);

/* Serial port functions */
void serial_init(uint16_t port);
void serial_putc(uint16_t port, char c);
void serial_puts(uint16_t port, const char *str);

/* VGA functions */
void vga_putc(char c);
void vga_puts(const char *str);
void vga_set_color(uint8_t fg, uint8_t bg);

#endif /* HAL_H */