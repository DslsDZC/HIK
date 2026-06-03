#include "lib/console.h"
#include <stdio.h>

void console_init(console_type_t type) { (void)type; }
void console_set_serial_config(u16 port, u32 baud) { (void)port; (void)baud; }
void console_putchar(char c) { putchar(c); }
void console_puts(const char *str) { fputs(str, stdout); }
void console_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
void console_vprintf(const char *fmt, va_list args) { vprintf(fmt, args); }
void console_clear(void) {}
void console_puthex64(u64 value) { printf("0x%016lx", value); }
void console_puthex32(u32 value) { printf("0x%08x", value); }
void console_putu64(u64 value) { printf("%lu", value); }
void console_putu32(u32 value) { printf("%u", value); }
void console_puti64(s64 value) { printf("%ld", value); }
void console_puti32(s32 value) { printf("%d", value); }
