/*
 * HIK Core-1 stddef.h
 *
 * Local implementation of standard definitions.
 * This replaces the standard library stddef.h.
 */

#ifndef HIK_CORE1_STDDEF_H
#define HIK_CORE1_STDDEF_H

/* NULL pointer constant */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* Size type */
typedef unsigned long size_t;

/* Pointer difference type */
typedef long ptrdiff_t;

/* offsetof macro */
#define offsetof(type, member) ((size_t)&(((type*)0)->member))

#endif /* HIK_CORE1_STDDEF_H */