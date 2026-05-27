/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86_64 PIT 定时器初始化
 *
 * 配置 PIT 通道 0 以固定频率产生 IRQ 0，
 * 中断路由至 scheduler_tick 作为 Core-0 的调度机制起点。
 * P1 调度器上线后可接管定时器管理。
 */

#include "hal.h"
#include "irq.h"
#include "thread.h"

/* 8259 PIC I/O 端口 */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

/* PIT I/O 端口 */
#define PIT_CH0      0x40
#define PIT_CMD      0x43

/* PIT 时钟频率（Hz） */
#define PIT_BASE_FREQ 1193182ULL

/* 调度器滴答频率（Hz） */
#define SCHED_TICK_HZ 1000

void timer_init(void)
{
    /* ICW1: 初始化 PIC1/PIC2 */
    hal_outb(PIC1_CMD, 0x11);
    hal_outb(PIC2_CMD, 0x11);

    /* ICW2: 设置 IRQ 基向量（IRQ 0→32, IRQ 8→40） */
    hal_outb(PIC1_DATA, 0x20);
    hal_outb(PIC2_DATA, 0x28);

    /* ICW3: 级联连接 */
    hal_outb(PIC1_DATA, 0x04);
    hal_outb(PIC2_DATA, 0x02);

    /* ICW4: 8086 模式 */
    hal_outb(PIC1_DATA, 0x01);
    hal_outb(PIC2_DATA, 0x01);

    /* 屏蔽所有 PIC 中断（后续由 irq_table 逐个启用） */
    hal_outb(PIC1_DATA, 0xFF);
    hal_outb(PIC2_DATA, 0xFF);

    /* 配置 PIT 通道 0：模式 3（方波），二进制计数 */
    u16 divisor = (u16)(PIT_BASE_FREQ / SCHED_TICK_HZ);
    hal_outb(PIT_CMD, 0x36);
    hal_outb(PIT_CH0, divisor & 0xFF);
    hal_outb(PIT_CH0, (divisor >> 8) & 0xFF);

    /* IRQ 0 → scheduler_tick 已在 irq_table 静态初始化中定义 */
    /* 解除 IRQ 0 的屏蔽 */
    u8 mask = hal_inb(PIC1_DATA);
    hal_outb(PIC1_DATA, mask & ~0x01);
}
