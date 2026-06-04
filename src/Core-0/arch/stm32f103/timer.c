/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * STM32F103 SysTick 定时器驱动
 *
 * 配置 SysTick 以 1000Hz 产生周期中断，
 * 用于内核调度 tick（scheduler_tick）。
 *
 * SysTick 时钟来源：
 *   - HCLK/8 = 72MHz / 8 = 9MHz （默认）
 *   - SysTick 重装载值 = 9MHz / 1000Hz = 9000
 *
 * STK 寄存器（Cortex-M3 系统控制块）：
 *   0xE000E010: STK_CTRL  — 控制与状态
 *   0xE000E014: STK_LOAD  — 重装载值
 *   0xE000E018: STK_VAL   — 当前值
 */

#include "hal.h"
#include "irq.h"
#include "thread.h"
#include "lib/console.h"

/* SysTick 寄存器地址 */
#define STK_CTRL            (*(volatile uint32_t *)0xE000E010)
#define STK_LOAD            (*(volatile uint32_t *)0xE000E014)
#define STK_VAL             (*(volatile uint32_t *)0xE000E018)

/* STK_CTRL 位 */
#define STK_CTRL_ENABLE     (1 << 0)    /* SysTick 使能 */
#define STK_CTRL_TICKINT    (1 << 1)    /* 中断使能 */
#define STK_CTRL_CLKSOURCE  (1 << 2)    /* 时钟源: 0=HCLK/8, 1=HCLK */
#define STK_CTRL_COUNTFLAG  (1 << 16)   /* 计数到零标志 */

/* SysTick 时钟（HCLK/8 = 9MHz @ 72MHz HCLK） */
#define SYSTICK_CLOCK_HZ    9000000UL

/* 调度器 tick 频率 */
#define SCHED_TICK_HZ       1000

void timer_init(void)
{
    console_puts("[TIMER] Initializing SysTick at 1000Hz\n");

    /* 计算重装载值 */
    uint32_t reload = SYSTICK_CLOCK_HZ / SCHED_TICK_HZ;

    /* 禁止 SysTick 期间配置 */
    STK_CTRL &= ~STK_CTRL_ENABLE;

    /* 设置重装载值 */
    STK_LOAD = reload - 1;

    /* 清零当前值 */
    STK_VAL = 0;

    /*
     * 使能 SysTick：HCLK/8 时钟源 + 中断 + 使能
     *
     * 中断路由：SysTick 是 Cortex-M3 核内异常（异常号 15），
     * 固定由 NVIC 处理，不需要外部中断控制器路由。
     * SysTick handler 在 vectors.S 中注册。
     */
    STK_CTRL = STK_CTRL_CLKSOURCE  /* AHB/8 = 9MHz */
             | STK_CTRL_TICKINT    /* 使能中断 */
             | STK_CTRL_ENABLE;    /* 启动定时器 */

    console_puts("[TIMER] SysTick started, reload=");
    console_putu32(reload);
    console_puts("\n");
}
