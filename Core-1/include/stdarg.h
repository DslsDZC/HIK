/*
 * HIK Core-1 stdarg.h
 * 
 * Local implementation of variable argument list support.
 * This replaces the standard library stdarg.h.
 */

#ifndef HIK_CORE1_STDARG_H
#define HIK_CORE1_STDARG_H

/* Type definitions for variable arguments */
typedef __builtin_va_list va_list;

/* Macros for variable argument handling */
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif /* HIK_CORE1_STDARG_H */