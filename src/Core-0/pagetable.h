/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 页表管理器 — 架构无关接口
 *
 * page_table_t 为 512 条目数组（适配 4KB 页 × 4/5 级页表）。
 * 具体描述符格式由 arch/ 下的实现决定。
 */

#ifndef HIC_KERNEL_PAGETABLE_H
#define HIC_KERNEL_PAGETABLE_H

#include "types.h"
#include "domain.h"

/* PAGE_SIZE/PAGE_ALIGN 在 pmm.h 中定义 */

/* 架构页表标志（由 arch/<arch>/page_table_flags.h 提供） */
#include "page_table_flags.h"

typedef struct {
    u64 entries[512];
} __attribute__((aligned(4096))) page_table_t;

/* 映射类型 */
typedef enum {
    MAP_TYPE_IDENTITY,
    MAP_TYPE_KERNEL,
    MAP_TYPE_USER,
} map_type_t;

/* 页权限（架构无关，各 arch 映射到具体描述符位） */
typedef enum {
    PERM_NONE    = 0,
    PERM_READ    = 1,
    PERM_WRITE   = 2,
    PERM_EXEC    = 4,
    PERM_RW      = PERM_READ | PERM_WRITE,
    PERM_RX      = PERM_READ | PERM_EXEC,
    PERM_RWX     = PERM_READ | PERM_WRITE | PERM_EXEC,
} page_perm_t;

void pagetable_init(void);
page_table_t *pagetable_create(void);
void pagetable_destroy(page_table_t *root);

hic_status_t pagetable_map(page_table_t *root, virt_addr_t va, phys_addr_t pa,
                            size_t size, page_perm_t perm, map_type_t type);
hic_status_t pagetable_unmap(page_table_t *root, virt_addr_t va, size_t size);
hic_status_t pagetable_set_perm(page_table_t *root, virt_addr_t va,
                                size_t size, page_perm_t perm);
phys_addr_t pagetable_get_phys(page_table_t *root, virt_addr_t va);
void pagetable_switch(page_table_t *root);
void pagetable_flush_tlb(virt_addr_t addr);
void pagetable_flush_tlb_all(void);

/* 获取当前页表基址 */
page_table_t *pagetable_get_current(void);

hic_status_t pagetable_setup_domain(domain_id_t domain, page_table_t *root);
void pagetable_cleanup_domain(domain_id_t domain);

#endif /* HIC_KERNEL_PAGETABLE_H */
