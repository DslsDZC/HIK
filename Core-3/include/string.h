/*
 * HIK Core-3 String Library Header
 */

#ifndef HIK_CORE3_STRING_H
#define HIK_CORE3_STRING_H

#include "stdint.h"
#include "stddef.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

/* String functions */
size_t strlen(const char *str);
char* strcpy(char *dest, const char *src);
char* strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char* strcat(char *dest, const char *src);
char* strchr(const char *str, int c);

/* Memory functions */
void* memset(void *ptr, int value, size_t n);
void* memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *ptr1, const void *ptr2, size_t n);

#endif /* HIK_CORE3_STRING_H */