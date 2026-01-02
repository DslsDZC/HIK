/*
 * HIK Core-0 Debug Library
 */

#include "stdint.h"

/* VGA text mode constants */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

/* VGA colors */
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_WHITE 15

/* Current cursor position */
static int g_vga_pos = 0;

/* VGA color attribute (white on black) */
static const uint16_t g_vga_color = (VGA_COLOR_WHITE << 8) | VGA_COLOR_BLACK;

/*
 * Write a character to VGA
 */
static void vga_putchar(char c) {
    uint16_t *vga = (uint16_t*)VGA_MEMORY;

    if (c == '\n') {
        /* Newline - move to next line */
        g_vga_pos = (g_vga_pos / VGA_WIDTH + 1) * VGA_WIDTH;
        if (g_vga_pos >= VGA_WIDTH * VGA_HEIGHT) {
            /* Scroll up */
            for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
                vga[i] = vga[i + VGA_WIDTH];
            }
            /* Clear last line */
            for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
                vga[i] = ' ' | g_vga_color;
            }
            g_vga_pos -= VGA_WIDTH;
        }
    } else {
        /* Write character */
        vga[g_vga_pos++] = (uint16_t)c | g_vga_color;
        if (g_vga_pos >= VGA_WIDTH * VGA_HEIGHT) {
            g_vga_pos = 0;
        }
    }
}

/*
 * Write a string to VGA
 */
void debug_print(const char *str) {
    const char *p = str;

    while (*p) {
        vga_putchar(*p++);
    }
}

/*
 * Write a hex value to VGA
 */
void debug_print_hex(uint64_t value) {
    char hex[17];
    const char *digits = "0123456789ABCDEF";

    for (int i = 15; i >= 0; i--) {
        hex[i] = digits[value & 0xF];
        value >>= 4;
    }
    hex[16] = '\0';

    debug_print(hex);
}

/*
 * Clear the screen
 */
void debug_clear(void) {
    uint16_t *vga = (uint16_t*)VGA_MEMORY;

    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = ' ' | g_vga_color;
    }

    g_vga_pos = 0;
}
