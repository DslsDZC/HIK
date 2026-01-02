/*
 * HIK Core-3 String Library
 * 
 * Basic string manipulation functions for applications.
 */

#include "../include/string.h"

/* String length */
size_t strlen(const char *str) {
    if (!str) {
        return 0;
    }
    
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/* String copy */
char* strcpy(char *dest, const char *src) {
    if (!dest || !src) {
        return NULL;
    }
    
    char *d = dest;
    while ((*d++ = *src++) != '\0') {
        /* Copy until null terminator */
    }
    return dest;
}

/* String copy with size limit */
char* strncpy(char *dest, const char *src, size_t n) {
    if (!dest || !src) {
        return NULL;
    }
    
    char *d = dest;
    while (n > 0 && *src != '\0') {
        *d++ = *src++;
        n--;
    }
    
    /* Pad with nulls if needed */
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    
    return dest;
}

/* String compare */
int strcmp(const char *s1, const char *s2) {
    if (!s1 || !s2) {
        if (!s1 && !s2) return 0;
        return (!s1) ? -1 : 1;
    }
    
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* String compare with size limit */
int strncmp(const char *s1, const char *s2, size_t n) {
    if (!s1 || !s2) {
        if (!s1 && !s2) return 0;
        return (!s1) ? -1 : 1;
    }
    
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    
    if (n == 0) {
        return 0;
    }
    
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* String concatenate */
char* strcat(char *dest, const char *src) {
    if (!dest || !src) {
        return NULL;
    }
    
    char *d = dest;
    while (*d != '\0') {
        d++;
    }
    
    while ((*d++ = *src++) != '\0') {
        /* Concatenate until null terminator */
    }
    
    return dest;
}

/* Find character in string */
char* strchr(const char *str, int c) {
    if (!str) {
        return NULL;
    }
    
    while (*str != '\0') {
        if (*str == (char)c) {
            return (char*)str;
        }
        str++;
    }
    
    return NULL;
}

/* Memory set */
void* memset(void *ptr, int value, size_t n) {
    if (!ptr || n == 0) {
        return ptr;
    }
    
    unsigned char *p = (unsigned char*)ptr;
    unsigned char v = (unsigned char)value;
    
    while (n > 0) {
        *p++ = v;
        n--;
    }
    
    return ptr;
}

/* Memory copy */
void* memcpy(void *dest, const void *src, size_t n) {
    if (!dest || !src || n == 0) {
        return dest;
    }
    
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    
    while (n > 0) {
        *d++ = *s++;
        n--;
    }
    
    return dest;
}

/* Memory compare */
int memcmp(const void *ptr1, const void *ptr2, size_t n) {
    if (!ptr1 || !ptr2) {
        if (!ptr1 && !ptr2) return 0;
        return (!ptr1) ? -1 : 1;
    }
    
    const unsigned char *p1 = (const unsigned char*)ptr1;
    const unsigned char *p2 = (const unsigned char*)ptr2;
    
    while (n > 0) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
        n--;
    }
    
    return 0;
}