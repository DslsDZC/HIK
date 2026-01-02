/*
 * HIK Core-0 String and Memory Library
 */

#include "stdint.h"
#include "stddef.h"

/* Memory operations */
void* memcpy(void *dst, const void *src, uint64_t size) {
    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;
    
    while (size--) {
        *d++ = *s++;
    }
    
    return dst;
}

void* memset(void *ptr, uint8_t value, uint64_t size) {
    uint8_t *p = (uint8_t*)ptr;
    
    while (size--) {
        *p++ = value;
    }
    
    return ptr;
}

int memcmp(const void *ptr1, const void *ptr2, uint64_t size) {
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

/* String operations */
uint64_t strlen(const char *str) {
    uint64_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

int strcmp(const char *str1, const char *str2) {
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return *str1 - *str2;
}

int strncmp(const char *str1, const char *str2, uint64_t n) {
    while (n-- && *str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return (n == 0) ? 0 : *str1 - *str2;
}

char* strcpy(char *dst, const char *src) {
    char *orig_dst = dst;
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
    return orig_dst;
}

char* strncpy(char *dst, const char *src, uint64_t n) {
    char *orig_dst = dst;
    while (n-- && *src) {
        *dst++ = *src++;
    }
    while (n--) {
        *dst++ = '\0';
    }
    return orig_dst;
}

char* strchr(const char *str, int c) {
    while (*str) {
        if (*str == c) {
            return (char*)str;
        }
        str++;
    }
    return NULL;
}