/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * ARM64 GICv3 中断控制器初始化
 *
 * 支持 GICv3 系统寄存器接口（ICC_*_EL1），
 * 无需 MMIO 访问 Distributor/Redistributor（但保留 MMIO 回退路径）。
 */

#include "hal.h"
#include "irq.h"
#include "lib/console.h"

/* GIC 系统寄存器（ICC_*_EL1） */
#define ICC_PMR_EL1      "icc_pmr_el1"
#define ICC_IGRPEN0_EL1  "icc_igrpen0_el1"
#define ICC_IGRPEN1_EL1  "icc_igrpen1_el1"
#define ICC_IAR0_EL1     "icc_iar0_el1"
#define ICC_IAR1_EL1     "icc_iar1_el1"
#define ICC_EOIR0_EL1    "icc_eoir0_el1"
#define ICC_EOIR1_EL1    "icc_eoir1_el1"
#define ICC_BPR0_EL1     "icc_bpr0_el1"
#define ICC_BPR1_EL1     "icc_bpr1_el1"
#define ICC_SRE_EL1      "icc_sre_el1"

/* GIC MMIO 基址（QEMU virt 平台） */
#define GICD_BASE        0x08000000
#define GICR_BASE        0x080A0000

/* GIC Distributor 寄存器 */
#define GICD_CTLR        0x0000
#define GICD_TYPER       0x0004
#define GICD_ISENABLER   0x0100
#define GICD_ICENABLER   0x0180
#define GICD_ICPEND      0x0280
#define GICD_ICACTIVE    0x0380

/* GIC Redistributor 寄存器 */
#define GICR_WAKER       0x0014

static volatile u32 *gicd_base = (u32 *)GICD_BASE;

static inline u32 gicd_read(u32 off)
{
    return gicd_base[off / 4];
}

static inline void gicd_write(u32 off, u32 val)
{
    gicd_base[off / 4] = val;
}

static void gic_cpu_init(void)
{
    /* 启用 GIC 系统寄存器接口（ICC_SRE_EL1） */
    u32 sre;
    __asm__ volatile ("mrs %0, " ICC_SRE_EL1 : "=r"(sre));
    sre |= 1;  /* SRE=1: 启用系统寄存器接口 */
    __asm__ volatile ("msr " ICC_SRE_EL1 ", %0; isb" : : "r"(sre));

    /* 设置优先级掩码：接收所有中断 */
    __asm__ volatile ("msr " ICC_PMR_EL1 ", %0" : : "r"(0xFF));

    /* 设置二进制点：无优先级分组 */
    __asm__ volatile ("msr " ICC_BPR0_EL1 ", %0" : : "r"(0));
    __asm__ volatile ("msr " ICC_BPR1_EL1 ", %0" : : "r"(0));

    /* 启用组 0 和组 1 中断 */
    __asm__ volatile ("msr " ICC_IGRPEN0_EL1 ", %0" : : "r"(1));
    __asm__ volatile ("msr " ICC_IGRPEN1_EL1 ", %0; isb" : : "r"(1));
}

void gic_init(void)
{
    console_puts("[GIC] Initializing GICv3...\n");

    /* 唤醒 Redistributor */
    u32 waker = gicd_read(GICR_WAKER);
    waker &= ~2;  /* 清除 ProcessorSleep */
    gicd_write(GICR_WAKER, waker);
    while (gicd_read(GICR_WAKER) & 1);  /* 等待 ChildrenAsleep 清除 */

    /* 禁用所有中断（Distributor） */
    gicd_write(GICD_CTLR, 0);
    gicd_write(GICD_ICENABLER, 0xFFFFFFFF);
    gicd_write(GICD_ICENABLER + 4, 0xFFFFFFFF);
    gicd_write(GICD_ICENABLER + 8, 0xFFFFFFFF);
    gicd_write(GICD_ICENABLER + 12, 0xFFFFFFFF);

    /* 清除所有挂起 */
    gicd_write(GICD_ICPEND, 0xFFFFFFFF);
    gicd_write(GICD_ICPEND + 4, 0xFFFFFFFF);

    /* 初始化 CPU 接口 */
    gic_cpu_init();

    /* 启用 Distributor */
    gicd_write(GICD_CTLR, 1);

    console_puts("[GIC] GICv3 initialized\n");
}

void gic_enable_irq(u32 irq)
{
    if (irq >= 1024) return;

    /* 使用 SPI 或 PPI 的 ISENABLER 寄存器 */
    u32 reg_idx = irq / 32;
    u32 bit = irq % 32;
    gicd_write(GICD_ISENABLER + reg_idx * 4, 1U << bit);
}

void gic_disable_irq(u32 irq)
{
    if (irq >= 1024) return;
    u32 reg_idx = irq / 32;
    u32 bit = irq % 32;
    gicd_write(GICD_ICENABLER + reg_idx * 4, 1U << bit);
}

u32 gic_read_iar(void)
{
    u32 iar;
    __asm__ volatile ("mrs %0, " ICC_IAR1_EL1 : "=r"(iar));
    return iar;
}

void gic_write_eoi(u32 irq)
{
    __asm__ volatile ("msr " ICC_EOIR1_EL1 ", %0" : : "r"(irq));
}
