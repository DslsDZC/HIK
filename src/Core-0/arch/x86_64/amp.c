/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86_64 AMP — LAPIC SIPI 多核启动
 *
 * 使用 APIC 中断命令寄存器（ICR）发送 INIT-SIPI-SIPI 序列。
 * 从核通过 AP trampoline 进入保护模式 → 长模式 → C 入口。
 */

#include "amp.h"
#include "hal.h"
#include "pmm.h"
#include "boot_info.h"
#include "lib/console.h"
#include "lib/mem.h"

#define AP_TRAMPOLINE_ADDR   0x8000
#define LAPIC_BASE           0xFEE00000
#define LAPIC_ICR            0x300

extern void ap_trampoline(void);
extern char __ap_trampoline_start[];
extern char __ap_trampoline_end[];

static inline u32 lapic_read(u32 off) {
    return *(volatile u32 *)(uintptr_t)(LAPIC_BASE + off);
}
static inline void lapic_write(u32 off, u32 val) {
    *(volatile u32 *)(uintptr_t)(LAPIC_BASE + off) = val;
}

static void send_ipi(u32 apic_id, u32 vector) {
    u32 icr = vector | (apic_id << 24) | (1 << 14);
    lapic_write(LAPIC_ICR, icr);
    u32 to = 10000;
    while (lapic_read(LAPIC_ICR) & (1 << 12)) {
        if (--to == 0) break;
        hal_idle();
    }
}

static hic_status_t prepare_trampoline(void) {
    extern char _kernel_end[], gdt_ptr[], gdt[], pml4_addr[], stack_top[];
    size_t lma = ((size_t)(uintptr_t)_kernel_end + 0xFFF) & ~((size_t)0xFFF);
    size_t sz = (size_t)(__ap_trampoline_end - __ap_trampoline_start);
    if (sz == 0 || sz > 4096) sz = 512;
    if (AP_TRAMPOLINE_ADDR >= 0x100000) return HIC_ERROR_INVALID_PARAM;
    for (size_t i = 0; i < sz; i++)
        ((u8 *)AP_TRAMPOLINE_ADDR)[i] = ((u8 *)lma)[i];
    u64 gdt_phys = AP_TRAMPOLINE_ADDR + ((uintptr_t)gdt - (uintptr_t)__ap_trampoline_start);
    u64 *base = (u64 *)(AP_TRAMPOLINE_ADDR + ((uintptr_t)gdt_ptr - (uintptr_t)__ap_trampoline_start) + 2);
    *base = gdt_phys;
    hal_memory_barrier();
    return HIC_SUCCESS;
}

hic_status_t arch_boot_aps(void) {
    hic_status_t st = prepare_trampoline();
    if (st != HIC_SUCCESS) return st;

    for (u32 i = 1; i < g_amp_info.cpu_count; i++) {
        phys_addr_t stack;
        if (pmm_alloc_frames(0, 2, PAGE_FRAME_CORE, &stack) != HIC_SUCCESS) continue;
        g_amp_info.cpus[i].stack_base = (void *)stack;
        g_amp_info.cpus[i].stack_top = (void *)(stack + 8192);
        hal_memory_barrier();
        send_ipi(i, AP_TRAMPOLINE_ADDR >> 12);
        u32 to = 10000;
        while (to > 0 && g_amp_info.cpus[i].state != AMP_CPU_ONLINE) {
            hal_udelay(10); to -= 10;
        }
        if (g_amp_info.cpus[i].state == AMP_CPU_ONLINE) g_amp_info.online_cpus++;
    }
    hal_memory_barrier();
    return HIC_SUCCESS;
}
