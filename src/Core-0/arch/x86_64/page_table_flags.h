/*
 * x86_64 页表描述符位定义
 * 通过 pagetable.h 间接包含
 */

#ifndef HIC_ARCH_PAGE_TABLE_FLAGS_H
#define HIC_ARCH_PAGE_TABLE_FLAGS_H

#define PAGE_FLAG_PRESENT   (1ULL << 0)
#define PAGE_FLAG_WRITE     (1ULL << 1)
#define PAGE_FLAG_USER      (1ULL << 2)
#define PAGE_FLAG_NX        (1ULL << 63)

#endif
