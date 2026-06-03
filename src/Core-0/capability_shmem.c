/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC能力系统 - 共享内存机制层实现
 *
 * 提供共享内存区域的分配、映射、解映射和信息查询。
 * 基于能力系统实现权限验证和引用计数管理。
 */

#include "capability.h"
#include "domain.h"
#include "pmm.h"
#include "hal.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"

/* ==================== 共享内存机制层实现 ==================== */

/**
 * 分配共享内存区域（机制层）
 */
hic_status_t shmem_alloc(domain_id_t owner, size_t size, u32 flags,
                          cap_id_t *out_cap, cap_handle_t *out_handle) {
    if (owner >= HIC_DOMAIN_MAX || size == 0 || !out_cap || !out_handle) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    /* 查找空闲共享内存槽 */
    u32 region_idx = MAX_SHMEM_REGIONS;
    for (u32 i = 0; i < MAX_SHMEM_REGIONS; i++) {
        if (g_shmem_regions[i].phys_base == 0) {
            region_idx = i;
            break;
        }
    }

    if (region_idx == MAX_SHMEM_REGIONS) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_RESOURCE;
    }

    /* 分配物理内存 */
    phys_addr_t phys_base;
    u32 pages = (u32)((size + PAGE_SIZE - 1) / PAGE_SIZE);
    if (pmm_alloc_frames(owner, pages, PAGE_FRAME_SHARED, &phys_base) != HIC_SUCCESS) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_MEMORY;
    }

    /* 计算权限 */
    cap_rights_t rights = CAP_MEM_READ;
    if (flags & SHMEM_FLAG_WRITABLE) {
        rights |= CAP_MEM_WRITE;
    }

    /* 创建内存能力 */
    cap_id_t cap;
    hic_status_t status = cap_create_memory(owner, phys_base, pages * PAGE_SIZE, rights, &cap);
    if (status != HIC_SUCCESS) {
        pmm_free_frames(phys_base, pages);
        atomic_exit_critical(irq);
        return status;
    }

    /* 记录共享内存区域 */
    g_shmem_regions[region_idx].phys_base = phys_base;
    g_shmem_regions[region_idx].size = pages * PAGE_SIZE;
    g_shmem_regions[region_idx].owner = owner;
    g_shmem_regions[region_idx].ref_count = 1;
    g_shmem_regions[region_idx].flags = flags;

    atomic_exit_critical(irq);

    /* 生成句柄给创建者 */
    status = cap_grant(owner, cap, out_handle);
    if (status != HIC_SUCCESS) {
        /* 回滚 */
        cap_revoke(cap);
        pmm_free_frames(phys_base, pages);
        g_shmem_regions[region_idx].phys_base = 0;
        return status;
    }

    *out_cap = cap;
    return HIC_SUCCESS;
}

/**
 * 映射共享内存到目标域（机制层）
 */
hic_status_t shmem_map(domain_id_t from, domain_id_t to, cap_id_t cap,
                        cap_rights_t rights, cap_handle_t *out_handle) {
    if (from >= HIC_DOMAIN_MAX || to >= HIC_DOMAIN_MAX ||
        cap >= CAP_TABLE_SIZE || !out_handle) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cap_entry_t *entry = &g_global_cap_table[cap];

    /* 检查能力有效性 */
    if (entry->cap_id != cap || (entry->flags & CAP_FLAG_REVOKED)) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    /* 检查发送方持有该能力 */
    if (entry->owner != from) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;
    }

    /* 检查权限单调性：授予的权限不能超过原权限 */
    if ((rights & entry->rights) != rights) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;
    }

    /* 使用带衰减的传递机制 */
    hic_status_t status = cap_transfer_with_attenuation(from, to, cap, rights, out_handle);

    if (status == HIC_SUCCESS) {
        /* 更新引用计数 */
        for (u32 i = 0; i < MAX_SHMEM_REGIONS; i++) {
            if (g_shmem_regions[i].phys_base == entry->memory.base) {
                g_shmem_regions[i].ref_count++;
                break;
            }
        }
    }

    atomic_exit_critical(irq);

    return status;
}

/**
 * 解除共享内存映射（机制层）
 */
hic_status_t shmem_unmap(domain_id_t domain, cap_handle_t handle) {
    cap_id_t cap_id = cap_get_cap_id(handle);

    if (cap_id >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }

    bool irq = atomic_enter_critical();

    cap_entry_t *entry = &g_global_cap_table[cap_id];

    /* 验证令牌 */
    u32 token = cap_get_token(handle);
    if (!cap_validate_token(domain, cap_id, token)) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }

    /* 检查所有权 */
    if (entry->owner != domain) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;
    }

    /* 更新引用计数 */
    for (u32 i = 0; i < MAX_SHMEM_REGIONS; i++) {
        if (g_shmem_regions[i].phys_base == entry->memory.base) {
            if (g_shmem_regions[i].ref_count > 0) {
                g_shmem_regions[i].ref_count--;
            }

            /* 如果引用计数为0，释放物理内存 */
            if (g_shmem_regions[i].ref_count == 0) {
                u32 pages = (u32)((g_shmem_regions[i].size + PAGE_SIZE - 1) / PAGE_SIZE);
                pmm_free_frames(g_shmem_regions[i].phys_base, pages);
                g_shmem_regions[i].phys_base = 0;
            }
            break;
        }
    }

    /* 撤销能力并清零 cap_id，使槽位可重用 */
    entry->flags = 0;
    entry->rights = 0;
    entry->owner = 0;
    entry->cap_id = 0;  /* 清零使槽位可重用 */

    atomic_exit_critical(irq);

    return HIC_SUCCESS;
}

/**
 * 获取共享内存信息
 */
hic_status_t shmem_get_info(cap_id_t cap, shmem_region_t *info) {
    if (cap >= CAP_TABLE_SIZE || !info) {
        return HIC_ERROR_INVALID_PARAM;
    }

    cap_entry_t *entry = &g_global_cap_table[cap];

    if (entry->cap_id != cap) {
        return HIC_ERROR_CAP_INVALID;
    }

    /* 查找对应的共享内存区域 */
    for (u32 i = 0; i < MAX_SHMEM_REGIONS; i++) {
        if (g_shmem_regions[i].phys_base == entry->memory.base) {
            *info = g_shmem_regions[i];
            return HIC_SUCCESS;
        }
    }

    return HIC_ERROR_NOT_FOUND;
}
