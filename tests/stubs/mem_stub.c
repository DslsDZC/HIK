#include "lib/mem.h"

void *memzero(void *ptr, size_t len) {
    unsigned char *p = (unsigned char *)ptr;
    for (size_t i = 0; i < len; i++) p[i] = 0;
    return ptr;
}

void *memcopy(void *dest, const void *src, size_t len) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < len; i++) d[i] = s[i];
    return dest;
}

int memcmp(const void *ptr1, const void *ptr2, size_t len) {
    const unsigned char *a = (const unsigned char *)ptr1;
    const unsigned char *b = (const unsigned char *)ptr2;
    for (size_t i = 0; i < len; i++) {
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    }
    return 0;
}

void *memmove(void *dest, const void *src, size_t len) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        for (size_t i = 0; i < len; i++) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = len; i > 0; i--) d[i - 1] = s[i - 1];
    }
    return dest;
}

void *memset(void *ptr, int value, size_t len) {
    unsigned char *p = (unsigned char *)ptr;
    for (size_t i = 0; i < len; i++) p[i] = (unsigned char)value;
    return ptr;
}

uint16_t swap16(uint16_t v) { return __builtin_bswap16(v); }
uint32_t swap32(uint32_t v) { return __builtin_bswap32(v); }
uint64_t swap64(uint64_t v) { return __builtin_bswap64(v); }
uintptr_t align_up(uintptr_t v, uintptr_t a) { return (v + a - 1) & ~(a - 1); }
uintptr_t align_down(uintptr_t v, uintptr_t a) { return v & ~(a - 1); }
void __stack_chk_fail(void) { while (1); }
