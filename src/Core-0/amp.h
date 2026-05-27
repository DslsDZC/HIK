/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * AMP（非对称多处理）—— 内核级实现
 *
 * AMP 仅处理 Core-0 内部维护任务：
 * - 周期性内核维护（审计刷新、监控统计）
 * - AP 启动与管理
 *
 * 所有上层计算资源分配由逻辑核心系统（logical_core.c）管理。
 * P1 服务和 Application 通过逻辑核心获取计算资源，不直接接触 AMP。
 */

#ifndef HIC_KERNEL_AMP_H
#define HIC_KERNEL_AMP_H

#include "types.h"
#include "hal.h"

#define MAX_CPUS                256

/* CPU 状态 */
typedef enum {
    AMP_CPU_OFFLINE,
    AMP_CPU_ONLINE,
} amp_cpu_state_t;

/* AP 数据结构 */
typedef struct {
    cpu_id_t cpu_id;
    amp_cpu_state_t state;
    void *stack_base;
    void *stack_top;
    u32 apic_id;
    u32 lapic_address;
    u64 tasks_completed;
} amp_cpu_t;

/* 全局 AMP 信息 */
typedef struct {
    amp_cpu_t cpus[MAX_CPUS];
    u32 cpu_count;
    u32 online_cpus;
    cpu_id_t bsp_id;
    bool amp_enabled;
} amp_info_t;

extern amp_info_t g_amp_info;
extern bool g_amp_enabled;

/* 架构 AP 启动钩子（arch/<arch>/amp.c 实现） */
hic_status_t arch_boot_aps(void) __attribute__((weak));

void amp_init(void);
hic_status_t amp_boot_aps(void);
void amp_wait_for_aps(void);
void amp_ap_main(void);
bool amp_is_enabled(void);
void amp_get_stats(cpu_id_t cpu_id, u64 *tasks_completed,
                    u64 *caps_verified, u64 *irqs_handled);

/* AP 后台任务产出（BSP 调用） */
phys_addr_t amp_pop_zeroed_page(void);
void amp_get_pmm_hint(phys_addr_t *base, u64 *count);

#endif /* HIC_KERNEL_AMP_H */
