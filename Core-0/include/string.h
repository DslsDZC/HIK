/*
 * HIK Core-0 String Library Header
 */

#ifndef HIK_CORE0_STRING_H
#define HIK_CORE0_STRING_H

#include "stdint.h"
#include "stddef.h"

/* String length */
size_t strlen(const char *str);

/* String copy */
char* strcpy(char *dest, const char *src);
char* strncpy(char *dest, const char *src, size_t n);

/* String compare */
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

/* String concatenate */
char* strcat(char *dest, const char *src);

/* Memory operations */
void* memset(void *ptr, int value, size_t n);
void* memcpy(void *dest, const void *src, size_t n);
void* memmove(void *dest, const void *src, size_t n);
int memcmp(const void *ptr1, const void *ptr2, size_t n);

#endif /* HIK_CORE0_STRING_H */