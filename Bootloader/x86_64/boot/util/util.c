/*
 * HIK BIOS Bootloader - Utility Functions Implementation
 */

#include "util.h"
#include <stddef.h>

/*
 * Copy memory
 */
void *memcpy(void *dst, const void *src, uint32_t size) {
    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;

    while (size--) {
        *d++ = *s++;
    }

    return dst;
}

/*
 * Set memory
 */
void *memset(void *ptr, int value, uint32_t size) {
    uint8_t *p = (uint8_t*)ptr;

    while (size--) {
        *p++ = (uint8_t)value;
    }

    return ptr;
}

/*
 * Compare memory
 */
int memcmp(const void *ptr1, const void *ptr2, uint32_t size) {
    const uint8_t *p1 = (const uint8_t*)ptr1;
    const uint8_t *p2 = (const uint8_t*)ptr2;

    while (size--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }

    return 0;
}

/*
 * Copy string
 */
char *strcpy(char *dst, const char *src) {
    char *orig_dst = dst;
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
    return orig_dst;
}

/*
 * Compare strings
 */
int strcmp(const char *str1, const char *str2) {
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return *str1 - *str2;
}

/*
 * Find character in string
 */
char *strchr(const char *str, int c) {
    while (*str) {
        if (*str == (char)c) {
            return (char*)str;
        }
        str++;
    }
    return NULL;
}

/*
 * Get string length
 */
size_t strlen(const char *str) {
    size_t len = 0;
    while (*str++) {
        len++;
    }
    return len;
}

/*
 * Convert string to unsigned long
 */
uint64_t strtoul(const char *str, char **endptr, int base) {
    uint64_t value = 0;
    
    if (base == 0) {
        if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
            base = 16;
            str += 2;
        } else if (str[0] == '0') {
            base = 8;
            str++;
        } else {
            base = 10;
        }
    }
    
    while (*str) {
        uint8_t digit;
        
        if (*str >= '0' && *str <= '9') {
            digit = *str - '0';
        } else if (*str >= 'a' && *str <= 'f') {
            digit = *str - 'a' + 10;
        } else if (*str >= 'A' && *str <= 'F') {
            digit = *str - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        value = value * base + digit;
        str++;
    }
    
    if (endptr) {
        *endptr = (char*)str;
    }
    
    return value;
}

/*
 * Simple delay using busy wait
 */
void delay(uint32_t milliseconds) {
    /* Very rough approximation */
    for (uint32_t i = 0; i < milliseconds * 1000; i++) {
        asm volatile("nop");
    }
}