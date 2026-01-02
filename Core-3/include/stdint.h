/*
 * HIK Core-3 stdint.h
 * 
 * Local implementation of standard integer types.
 * This replaces the standard library stdint.h.
 */

#ifndef HIK_CORE3_STDINT_H
#define HIK_CORE3_STDINT_H

/* Exact-width integer types */
typedef signed char         int8_t;
typedef unsigned char       uint8_t;
typedef short               int16_t;
typedef unsigned short      uint16_t;
typedef int                 int32_t;
typedef unsigned int        uint32_t;
typedef long long           int64_t;
typedef unsigned long long  uint64_t;

/* Minimum-width integer types */
typedef signed char         int_least8_t;
typedef unsigned char       uint_least8_t;
typedef short               int_least16_t;
typedef unsigned short      uint_least16_t;
typedef int                 int_least32_t;
typedef unsigned int        uint_least32_t;
typedef long long           int_least64_t;
typedef unsigned long long  uint_least64_t;

/* Fastest minimum-width integer types */
typedef signed char         int_fast8_t;
typedef unsigned char       uint_fast8_t;
typedef int                 int_fast16_t;
typedef unsigned int        uint_fast16_t;
typedef int                 int_fast32_t;
typedef unsigned int        uint_fast32_t;
typedef long long           int_fast64_t;
typedef unsigned long long  uint_fast64_t;

/* Pointer-holding integer types */
typedef long long           intptr_t;
typedef unsigned long long  uintptr_t;

/* Maximum-width integer types */
typedef long long           intmax_t;
typedef unsigned long long  uintmax_t;

/* Limits */
#define INT8_MIN         (-128)
#define INT16_MIN        (-32768)
#define INT32_MIN        (-2147483647 - 1)
#define INT64_MIN        (-9223372036854775807LL - 1)

#define INT8_MAX         127
#define INT16_MAX        32767
#define INT32_MAX        2147483647
#define INT64_MAX        9223372036854775807LL

#define UINT8_MAX        255
#define UINT16_MAX       65535
#define UINT32_MAX       4294967295U
#define UINT64_MAX       18446744073709551615ULL

#endif /* HIK_CORE3_STDINT_H */