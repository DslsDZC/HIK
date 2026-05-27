/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * AMP — 架构无关管理层
 *
 * AP 启动（arch_boot_aps）由 arch/<arch>/amp.c 实现。
 * 本文件包含：预清零页池、PMM 扫描提示、AP 后台任务循环。
 */

#include "amp.h"
#include "hal.h"
#include "pmm.h"
#include "boot_info.h"
#include "lib/console.h"
#include "lib/mem.h"

extern boot_state_t g_boot_state;
bool g_amp_enabled = false;
amp_info_t g_amp_info = {0};

/* 架构 AP 启动钩子 */
__attribute__((weak)) hic_status_t arch_boot_aps(void) { return HIC_ERROR_NOT_SUPPORTED; }

/* ===== 预清零页池 ===== */
#define ZEROED_POOL_SIZE  64
static phys_addr_t g_zeroed_pool[ZEROED_POOL_SIZE];
static volatile u32 g_zeroed_head = 0;
static volatile u32 g_zeroed_tail = 0;
static volatile u32 g_zeroed_count = 0;

static void zeroed_pool_put(phys_addr_t pa) {
    if (g_zeroed_count >= ZEROED_POOL_SIZE) return;
    g_zeroed_pool[g_zeroed_tail] = pa;
    g_zeroed_tail = (g_zeroed_tail + 1) % ZEROED_POOL_SIZE;
    hal_memory_barrier();
    g_zeroed_count++;
}

phys_addr_t amp_pop_zeroed_page(void) {
    if (g_zeroed_count == 0) return 0;
    phys_addr_t pa = g_zeroed_pool[g_zeroed_head];
    g_zeroed_head = (g_zeroed_head + 1) % ZEROED_POOL_SIZE;
    hal_memory_barrier();
    g_zeroed_count--;
    return pa;
}

/* ===== PMM 扫描提示 ===== */
static phys_addr_t g_pmm_hint_base = 0;
static u64 g_pmm_hint_count = 0;

void amp_get_pmm_hint(phys_addr_t *base, u64 *count) {
    *base = g_pmm_hint_base; *count = g_pmm_hint_count;
}

static void pmm_scan_task(void) {
    pmm_scan_largest_free(&g_pmm_hint_base, &g_pmm_hint_count);
    hal_memory_barrier();
}

/* ===== AP 后台轮转 ===== */
#define TASK_PREZERO  0
#define TASK_PMMSCAN  1
#define TASK_FVCHECK  2
#define TASK_COUNT    3
#define AP_TICK       1000000ULL

void amp_init(void) {
    memzero(&g_amp_info, sizeof(amp_info_t));
    for (u32 i = 0; i < MAX_CPUS; i++)
        g_amp_info.cpus[i].state = AMP_CPU_OFFLINE;
    g_amp_info.bsp_id = 0;
    g_amp_info.cpus[0].state = AMP_CPU_ONLINE;
    g_amp_info.cpu_count = (g_boot_state.valid && g_boot_state.hw.cpu.logical_cores > 0)
                           ? g_boot_state.hw.cpu.logical_cores : 1;
    g_amp_info.online_cpus = 1;
}

hic_status_t amp_boot_aps(void) {
    if (g_amp_info.cpu_count <= 1) return HIC_SUCCESS;
    hic_status_t st = arch_boot_aps();
    if (st == HIC_SUCCESS) {
        g_amp_info.amp_enabled = true;
        g_amp_enabled = true;
    }
    return st;
}

void amp_wait_for_aps(void) {
    for (u32 i = 1; i < g_amp_info.cpu_count; i++) {
        if (g_amp_info.cpus[i].state == AMP_CPU_OFFLINE) continue;
        for (u32 d = 0; d < 1000000; d++) {
            hal_udelay(1);
            if (g_amp_info.cpus[i].state == AMP_CPU_ONLINE) break;
        }
    }
}

bool amp_is_enabled(void) { return g_amp_info.amp_enabled; }

void amp_get_stats(cpu_id_t id, u64 *tc, u64 *cv, u64 *ih) {
    if (id >= MAX_CPUS) { if (tc) *tc = 0; if (cv) *cv = 0; if (ih) *ih = 0; return; }
    if (tc) *tc = g_amp_info.cpus[id].tasks_completed;
    if (cv) *cv = 0;
    if (ih) *ih = 0;
}

void amp_ap_main(void) {
    cpu_id_t id = hal_get_cpu_id();
    g_amp_info.cpus[id].state = AMP_CPU_ONLINE;
    hal_memory_barrier();
    u64 tick = 0;
    while (1) {
        kernel_maintenance_tasks();
        switch (tick % TASK_COUNT) {
        case TASK_PREZERO: {
            phys_addr_t pa;
            if (pmm_alloc_frames(0, 1, PAGE_FRAME_CORE, &pa) == HIC_SUCCESS) {
                memzero((void *)pa, 4096);
                zeroed_pool_put(pa);
            }
            break;
        }
        case TASK_PMMSCAN: pmm_scan_task(); break;
        case TASK_FVCHECK: break;
        }
        tick++;
        g_amp_info.cpus[id].tasks_completed++;
        for (volatile u64 d = 0; d < AP_TICK; d++) hal_idle();
    }
}
