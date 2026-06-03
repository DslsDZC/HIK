/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC能力系统 - 特权内存访问通道
 *
 * 为 Privileged-1 服务提供直接物理内存访问能力。
 * 包含特权域管理、地址范围检查和访问审计。
 */

#include "capability.h"
#include "domain.h"
#include "hal.h"
#include "atomic.h"

/* ==================== 特权内存访问通道实现（增强安全版） ==================== */

/* Core-0 内存区域（绝对禁止访问） */
phys_addr_t g_core0_mem_start = 0x00000000;
phys_addr_t g_core0_mem_end   = 0x00FFFFFF;

/* 可用物理内存范围（防止访问未映射区域） */
phys_addr_t g_usable_memory_start = 0x01000000;
phys_addr_t g_usable_memory_end   = 0xFFFFFFFF;

/* 特权域位图（运行时验证，防止标志位被篡改） */
u32 g_privileged_domain_bitmap[HIC_DOMAIN_MAX / 32];

/* 特权访问审计计数器（已废弃：使用 domain_t.audit_counters.privileged_mem_access) */
u64 g_privileged_access_count[HIC_DOMAIN_MAX] __attribute__((deprecated));

/* 检查域是否为特权域（使用位图快速查找） */
bool cap_is_privileged_domain(domain_id_t domain) {
    if (domain >= HIC_DOMAIN_MAX) {
        return false;
    }

    u32 bitmap_index = domain / 32;
    u32 bitmap_bit = 1U << (domain % 32);

    return (g_privileged_domain_bitmap[bitmap_index] & bitmap_bit) != 0;
}

/* 设置特权域标志 */
void cap_set_privileged_domain(domain_id_t domain, bool privileged) {
    if (domain >= HIC_DOMAIN_MAX) {
        return;
    }

    bool irq = atomic_enter_critical();

    u32 bitmap_index = domain / 32;
    u32 bitmap_bit = 1U << (domain % 32);

    if (privileged) {
        g_privileged_domain_bitmap[bitmap_index] |= bitmap_bit;
    } else {
        g_privileged_domain_bitmap[bitmap_index] &= ~bitmap_bit;
    }

    atomic_exit_critical(irq);
}

/* 特权物理地址转虚拟地址（恒等映射） */
void *privileged_phys_to_virt(phys_addr_t addr) {
    return (void *)addr;
}

/* 特权内存访问检查（完整版本，带审计） */
bool privileged_check_access(domain_id_t domain, phys_addr_t addr, cap_rights_t access_type __attribute__((unused))) {
    /* 1. Core-0域始终允许访问所有特权区域（包括自己的内存） */
    if (domain == HIC_DOMAIN_CORE) {
        /* Core-0访问Core-0自己的内存，通过能力系统检查权限 */
        if (addr >= g_usable_memory_start && addr < g_usable_memory_end) {
            return true;
        }
        return false;  /* 访问Core-0自己的内存，应通过能力系统检查权限 */
    }

    /* 2. 检查是否在可用内存范围 */
    if (addr < g_usable_memory_start || addr >= g_usable_memory_end) {
        return false;
    }

    /* 3. 检查域是否为特权域（运行时验证） */
    if (!cap_is_privileged_domain(domain)) {
        return false;
    }

    /* 4. 记录访问计数（使用域私有审计计数器） */
    if (domain < HIC_DOMAIN_MAX && g_domains[domain].state != DOMAIN_STATE_INIT) {
        g_domains[domain].audit_counters.privileged_mem_access++;
    }

    return true;
}
