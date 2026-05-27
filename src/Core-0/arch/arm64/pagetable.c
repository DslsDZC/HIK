/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * ARM64 页表管理
 *
 * 4KB 页，4 级页表（L0-L3），TTBR0_EL1。
 * 与 x86_64 共用 page_table_t（512 u64 条目）。
 * 描述符格式不同（ARM64 Stage 1, EL1）。
 */

#include "pagetable.h"
#include "pmm.h"
#include "hal.h"
#include "domain.h"
#include "domain_switch.h"
#include "lib/mem.h"
#include "lib/console.h"

/* ARM64 页表描述符位 */
#define ARM64_PT_VALID      (1ULL << 0)
#define ARM64_PT_TABLE      (1ULL << 1)   /* L0-L2: table = 1, block = 0 */
#define ARM64_PT_PAGE       (1ULL << 1)   /* L3: page = 1 */
#define ARM64_PT_AF         (1ULL << 10)  /* Access flag */
#define ARM64_PT_AP_RW      (0ULL << 6)   /* EL1 RW, EL0 none */
#define ARM64_PT_AP_RO      (1ULL << 6)   /* EL1 RO, EL0 none */
#define ARM64_PT_AP_EL0RW   (1ULL << 6)   /* AP[2]=0, AP[1]=1: RW at both */
#define ARM64_PT_AP_EL0RO   (3ULL << 6)   /* AP[2]=1, AP[1]=1: RO at both */
#define ARM64_PT_UXN        (1ULL << 54)  /* Unprivileged execute never */
#define ARM64_PT_PXN        (1ULL << 53)  /* Privileged execute never */
#define ARM64_PT_NG         (1ULL << 11)  /* Not global */
#define ARM64_PT_SH_INNER   (3ULL << 8)   /* Inner shareable */

/* MAIR 属性索引 */
#define MAIR_ATTR_NORMAL    0
#define MAIR_ATTR_DEVICE    1

/* 页表操作 */
#define ARM64_PT_BLOCK2M    (ARM64_PT_VALID | ARM64_PT_AF)

static u64 arm64_pte_perm(page_perm_t perm, bool kernel)
{
    u64 pte = ARM64_PT_VALID | ARM64_PT_AF | ARM64_PT_SH_INNER | (MAIR_ATTR_NORMAL << 2);

    switch (perm & (PERM_READ | PERM_WRITE)) {
    case PERM_READ:
        pte |= ARM64_PT_AP_RO;
        break;
    case PERM_RW:
        pte |= ARM64_PT_AP_RW;
        break;
    default:
        pte |= ARM64_PT_AP_RW;
        break;
    }

    if (!(perm & PERM_EXEC))
        pte |= ARM64_PT_PXN | ARM64_PT_UXN;

    if (kernel)
        pte |= ARM64_PT_PXN;

    return pte;
}

static u64 *walk_create(page_table_t *root, u64 va, int level)
{
    u64 *entry = &root->entries[(va >> (39 - level * 9)) & 0x1FF];

    for (int lvl = 0; lvl < level; lvl++) {
        if (!(*entry & ARM64_PT_VALID)) {
            phys_addr_t new_pt_pa;
            if (pmm_alloc_frames(HIC_DOMAIN_CORE, 1, PAGE_FRAME_CORE, &new_pt_pa) != HIC_SUCCESS)
                return NULL;
            memzero((void *)new_pt_pa, 4096);
            *entry = new_pt_pa | ARM64_PT_VALID | ARM64_PT_TABLE | ARM64_PT_PXN | ARM64_PT_UXN;
        }
        u64 next_pa = *entry & ~0xFFFULL;
        entry = (u64 *)(next_pa + ((va >> (39 - (lvl + 1) * 9)) & 0x1FF) * 8);
    }
    return entry;
}

page_table_t *pagetable_create(void)
{
    phys_addr_t pa;
    if (pmm_alloc_frames(HIC_DOMAIN_CORE, 1, PAGE_FRAME_CORE, &pa) != HIC_SUCCESS)
        return NULL;
    memzero((void *)pa, 4096);
    return (page_table_t *)pa;
}

void pagetable_destroy(page_table_t *root)
{
    pmm_free_frames((phys_addr_t)root, 1);
}

hic_status_t pagetable_map(page_table_t *root, virt_addr_t va, phys_addr_t pa,
                            size_t size, page_perm_t perm, map_type_t type)
{
    bool kernel = (type == MAP_TYPE_KERNEL || type == MAP_TYPE_IDENTITY);
    u64 pte_flags = arm64_pte_perm(perm, kernel);

    for (u64 off = 0; off < size; off += 4096) {
        u64 *entry = walk_create(root, va + off, 3);
        if (!entry) return HIC_ERROR_NO_MEMORY;
        *entry = (pa + off) | pte_flags | ARM64_PT_PAGE;
    }
    hal_memory_barrier();
    return HIC_SUCCESS;
}

hic_status_t pagetable_unmap(page_table_t *root, virt_addr_t va, size_t size)
{
    for (u64 off = 0; off < size; off += 4096) {
        u64 *entry = walk_create(root, va + off, 3);
        if (entry) *entry = 0;
    }
    hal_memory_barrier();
    pagetable_flush_tlb(va);
    return HIC_SUCCESS;
}

phys_addr_t pagetable_get_phys(page_table_t *root, virt_addr_t va)
{
    u64 *entry = walk_create(root, va, 3);
    if (!entry || !(*entry & ARM64_PT_VALID)) return 0;
    return *entry & ~0xFFFULL;
}

void pagetable_switch(page_table_t *root)
{
    __asm__ volatile("msr ttbr0_el1, %0; isb; tlbi vmalle1; dsb sy; isb"
                     : : "r"((u64)root) : "memory");
}

page_table_t *pagetable_get_current(void) {
    u64 ttbr0;
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));
    return (page_table_t *)(ttbr0 & ~0xFFFULL);
}

void pagetable_flush_tlb(virt_addr_t addr)
{
    __asm__ volatile("tlbi vaae1is, %0; dsb sy; isb" : : "r"(addr) : "memory");
}

void pagetable_flush_tlb_all(void)
{
    __asm__ volatile("tlbi vmalle1is; dsb sy; isb" ::: "memory");
}

hic_status_t pagetable_set_perm(page_table_t *root, virt_addr_t va, size_t size, page_perm_t perm)
{
    u64 pte_flags = arm64_pte_perm(perm, true);
    for (u64 off = 0; off < size; off += 4096) {
        u64 *entry = walk_create(root, va + off, 3);
        if (entry && (*entry & ARM64_PT_VALID)) {
            *entry = (*entry & ~0xFFFULL) | pte_flags | ARM64_PT_PAGE;
        }
    }
    hal_memory_barrier();
    pagetable_flush_tlb_all();
    return HIC_SUCCESS;
}

hic_status_t pagetable_setup_domain(domain_id_t domain, page_table_t *root)
{
    extern void ipc3_map_domain_data(domain_id_t);
    extern phys_addr_t g_core0_mem_start, g_core0_mem_end;

    /* 映射域数据页 (0xFFFFF000) */
    ipc3_map_domain_data(domain);

    /* 恒等映射域代码/数据区域 */
    hic_status_t st;
    st = pagetable_map(root, 0, 0, g_core0_mem_start,
                       PERM_RWX, MAP_TYPE_IDENTITY);
    if (st != HIC_SUCCESS) return st;

    /* Core-0 内存标记为不可访问（已在页表中排除） */
    return HIC_SUCCESS;
}

void pagetable_cleanup_domain(domain_id_t domain)
{
    page_table_t *root = domain_switch_get_pagetable(domain);
    if (!root) return;
    extern phys_addr_t g_core0_mem_start;
    pagetable_unmap(root, 0, g_core0_mem_start);
    pagetable_destroy(root);
}

void pagetable_init(void) {}
