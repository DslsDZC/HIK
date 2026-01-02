/*
 * HIK Core-1 String Library Header
 */

#ifndef HIK_CORE1_STRING_H
#define HIK_CORE1_STRING_H

#include "stdint.h"
#include "stdarg.h"
#include "stddef.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

/* Type definitions */
typedef unsigned long size_t;

/* String functions */
size_t strlen(const char *str);
char* strcpy(char *dest, const char *src);
char* strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char* strcat(char *dest, const char *src);
char* strncat(char *dest, const char *src, size_t n);
char* strchr(const char *str, int c);
char* strrchr(const char *str, int c);
char* strstr(const char *haystack, const char *needle);

/* Memory functions */
void* memset(void *ptr, int value, size_t n);
void* memcpy(void *dest, const void *src, size_t n);
void* memmove(void *dest, const void *src, size_t n);
int memcmp(const void *ptr1, const void *ptr2, size_t n);

/* Formatted output (simplified) */
int snprintf(char *str, size_t size, const char *format, ...);

#endif /* HIK_CORE1_STRING_H */