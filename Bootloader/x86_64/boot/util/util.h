/*
 * HIK BIOS Bootloader - Utility Functions
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

/* Port I/O */
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outl(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

/* Memory operations */
void memcpy(void *dst, const void *src, uint64_t size);
void memset(void *ptr, uint8_t value, uint64_t size);
int memcmp(const void *ptr1, const void *ptr2, uint64_t size);
void strcpy(char *dst, const char *src);
int strcmp(const char *str1, const char *str2);
size_t strlen(const char *str);

/* String to number */
uint64_t strtoul(const char *str, char **endptr, int base);

/* Delay */
void delay(uint32_t milliseconds);

#endif /* UTIL_H */