/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * ARM64 AMP — PSCI 多核启动
 *
 * 通过 PSCI CPU_ON 唤醒从核，使用 HVC（EL2）或 SMC（EL3）调用。
 */

#include "amp.h"
#include "hal.h"
#include "pmm.h"

extern void _start(void);

#define PSCI_CPU_ON       0xC4000003

static u64 psci_call(u64 fn, u64 arg0, u64 arg1, u64 arg2) {
    /* PSCI calling convention: x0=fn, x1-3=args, x0=return */
    register u64 r0 asm("x0") = fn;
    register u64 r1 asm("x1") = arg0;
    register u64 r2 asm("x2") = arg1;
    register u64 r3 asm("x3") = arg2;
    __asm__ volatile("hvc #0"
                     : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r3)
                     : "memory");
    return r0;
}

hic_status_t arch_boot_aps(void) {
    if (g_amp_info.cpu_count <= 1) return HIC_SUCCESS;

    for (u32 i = 1; i < g_amp_info.cpu_count; i++) {
        phys_addr_t stack;
        if (pmm_alloc_frames(0, 2, PAGE_FRAME_CORE, &stack) != HIC_SUCCESS) continue;
        if (psci_call(PSCI_CPU_ON, i, (u64)_start, stack + 8192) == 0) {
            g_amp_info.cpus[i].state = AMP_CPU_ONLINE;
            g_amp_info.online_cpus++;
        }
    }
    g_amp_info.amp_enabled = true;
    g_amp_enabled = true;
    return HIC_SUCCESS;
}
