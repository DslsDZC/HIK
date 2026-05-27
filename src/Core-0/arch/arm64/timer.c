/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * ARM64 通用定时器初始化
 *
 * 使用 CNTPSIRQ/CNTP 物理计时器产生周期性调度滴答。
 * 触发 PPI（Private Peripheral Interrupt）中断，
 * 由 GIC 路由至当前 CPU。
 */

#include "hal.h"
#include "irq.h"
#include "thread.h"

#define CNTPCT_EL0     "cntpct_el0"
#define CNTP_CTL       "cntp_ctl_el0"
#define CNTP_CVAL      "cntp_cval_el0"
#define CNTP_TVAL      "cntp_tval_el0"
#define CNTFRQ_EL0     "cntfrq_el0"

#define SCHED_TICK_HZ  1000

void timer_init(void)
{
    /* 读取定时器频率 */
    u64 freq;
    __asm__ volatile ("mrs %0, " CNTFRQ_EL0 : "=r"(freq));
    if (freq == 0) freq = 24000000;  /* 默认 24MHz */

    /* 设置比较值：当前时间 + 一个滴答周期 */
    u64 tick_cycles = freq / SCHED_TICK_HZ;
    u64 now;
    __asm__ volatile ("mrs %0, " CNTPCT_EL0 : "=r"(now));

    u64 ctl_val = 1;  /* 启用定时器 */
    __asm__ volatile (
        "msr " CNTP_CVAL ", %0\n"
        "msr " CNTP_CTL ", %1\n"
        :
        : "r"(now + tick_cycles), "r"(ctl_val)
        : "memory"
    );

    /* PPI#30 → scheduler_tick 已在 irq_table 静态初始化中定义 */
    /* GIC 使能 PPI#30 */
    extern void gic_enable_irq(u32 irq);
    if (gic_enable_irq) gic_enable_irq(30);
}
