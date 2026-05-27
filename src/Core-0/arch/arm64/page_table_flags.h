/*
 * ARM64 页表描述符位定义
 * 通过 pagetable.h 间接包含，避免 pagetable.h 出现 #ifdef
 */

#ifndef HIC_ARCH_PAGE_TABLE_FLAGS_H
#define HIC_ARCH_PAGE_TABLE_FLAGS_H

#define PAGE_FLAG_PRESENT   (1ULL << 0)
#define PAGE_FLAG_WRITE     (1ULL << 1)   /* AP[2]=0 => RW, AP[2]=1 => RO */
#define PAGE_FLAG_USER      (1ULL << 6)   /* AP[1]=1 => EL0 accessible */
#define PAGE_FLAG_NX        (1ULL << 54)  /* XN: execute never */

#endif
