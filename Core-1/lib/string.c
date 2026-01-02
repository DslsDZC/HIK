/*
 * HIK Core-1 String Library
 * 
 * Basic string manipulation functions for Core-1 services.
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

/* String concatenate with size limit */
char* strncat(char *dest, const char *src, size_t n) {
    if (!dest || !src) {
        return NULL;
    }
    
    char *d = dest;
    while (*d != '\0') {
        d++;
    }
    
    while (n > 0 && *src != '\0') {
        *d++ = *src++;
        n--;
    }
    *d = '\0';
    
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

/* Find last occurrence of character in string */
char* strrchr(const char *str, int c) {
    if (!str) {
        return NULL;
    }
    
    const char *last = NULL;
    while (*str != '\0') {
        if (*str == (char)c) {
            last = str;
        }
        str++;
    }
    
    return (char*)last;
}

/* Find substring in string */
char* strstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) {
        return NULL;
    }
    
    if (*needle == '\0') {
        return (char*)haystack;
    }
    
    while (*haystack != '\0') {
        const char *h = haystack;
        const char *n = needle;
        
        while (*n != '\0' && *h == *n) {
            h++;
            n++;
        }
        
        if (*n == '\0') {
            return (char*)haystack;
        }
        
        haystack++;
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

/* Memory move */
void* memmove(void *dest, const void *src, size_t n) {
    if (!dest || !src || n == 0) {
        return dest;
    }
    
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    
    /* Check for overlap */
    if (d < s && d + n > s) {
        /* Destination overlaps source from beginning, copy backwards */
        d += n;
        s += n;
        while (n > 0) {
            *--d = *--s;
            n--;
        }
    } else if (s < d && s + n > d) {
        /* Source overlaps destination from beginning, copy backwards */
        d += n;
        s += n;
        while (n > 0) {
            *--d = *--s;
            n--;
        }
    } else {
        /* No overlap, copy forwards */
        while (n > 0) {
            *d++ = *s++;
            n--;
        }
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

/* Format string (simplified version) */
int snprintf(char *str, size_t size, const char *format, ...) {
    if (!str || !format || size == 0) {
        return -1;
    }
    
    /* This is a very simplified implementation that only handles
       basic string formatting. A full implementation would handle
       all format specifiers. */
    
    va_list args;
    va_start(args, format);
    
    size_t written = 0;
    const char *f = format;
    
    while (*f != '\0' && written < size - 1) {
        if (*f == '%') {
            f++;
            if (*f == '\0') break;
            
            if (*f == 's') {
                /* String */
                const char *s = va_arg(args, const char*);
                if (s) {
                    while (*s != '\0' && written < size - 1) {
                        str[written++] = *s++;
                    }
                }
            } else if (*f == 'd') {
                /* Integer */
                int val = va_arg(args, int);
                char buf[32];
                int i = 0;
                
                if (val == 0) {
                    buf[i++] = '0';
                } else {
                    int negative = 0;
                    if (val < 0) {
                        negative = 1;
                        val = -val;
                    }
                    
                    while (val > 0) {
                        buf[i++] = '0' + (val % 10);
                        val /= 10;
                    }
                    
                    if (negative) {
                        buf[i++] = '-';
                    }
                }
                
                while (i > 0 && written < size - 1) {
                    str[written++] = buf[--i];
                }
            } else if (*f == 'x') {
                /* Hexadecimal */
                unsigned int val = va_arg(args, unsigned int);
                char buf[32];
                int i = 0;
                
                if (val == 0) {
                    buf[i++] = '0';
                } else {
                    while (val > 0) {
                        int digit = val % 16;
                        buf[i++] = (digit < 10) ? '0' + digit : 'a' + digit - 10;
                        val /= 16;
                    }
                }
                
                while (i > 0 && written < size - 1) {
                    str[written++] = buf[--i];
                }
            } else if (*f == 'l') {
                f++;
                if (*f == 'x') {
                    /* Long hexadecimal */
                    unsigned long val = va_arg(args, unsigned long);
                    char buf[32];
                    int i = 0;
                    
                    if (val == 0) {
                        buf[i++] = '0';
                    } else {
                        while (val > 0) {
                            int digit = val % 16;
                            buf[i++] = (digit < 10) ? '0' + digit : 'a' + digit - 10;
                            val /= 16;
                        }
                    }
                    
                    while (i > 0 && written < size - 1) {
                        str[written++] = buf[--i];
                    }
                }
            } else {
                /* Unknown format, just copy the % and the character */
                if (written < size - 1) {
                    str[written++] = '%';
                }
                if (written < size - 1) {
                    str[written++] = *f;
                }
            }
            f++;
        } else {
            str[written++] = *f++;
        }
    }
    
    str[written] = '\0';
    
    va_end(args);
    
    return written;
}