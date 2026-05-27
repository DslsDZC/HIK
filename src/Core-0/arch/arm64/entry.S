/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC ARM64 内核入口点
 *
 * Bootloader 将 TLV 写入 0x1000 后跳转至此。
 * CPU 状态：EL1, MMU off, D-cache off, I-cache on.
 * x0 = 0（不使用设备树，统一从 0x1000 读 TLV）
 */

.section .text.start
.global _start

.extern kernel_start
.extern __stack_end
.extern __bss_start
.extern __bss_end

_start:
    /* 检查并降低到 EL1（QEMU 从 EL2 启动） */
    mrs x0, CurrentEL
    lsr x0, x0, #2
    cmp x0, #2
    b.lt 1f
    /* 当前在 EL2，降级到 EL1 */
    mov x0, #0x3C5           /* SPSR: EL1h, A/I/F masked */
    msr spsr_el2, x0
    adr x0, 1f
    msr elr_el2, x0
    eret                      /* 跳转到 EL1 的 1: 标签处 */
1:
    /* 设置栈指针 */
    ldr x0, =__stack_end
    mov sp, x0

    /* 清零 BSS */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
    sub x1, x1, x0
    lsr x1, x1, #3
    mov x2, #0
1:  str x2, [x0], #8
    sub x1, x1, #1
    cbnz x1, 1b

    /* 配置异常向量表 */
    ldr x0, =exception_vector_table
    msr VBAR_EL1, x0
    isb

    /* 启用 FP/SIMD */
    mrs x0, CPACR_EL1
    orr x0, x0, #(3 << 20)
    msr CPACR_EL1, x0
    isb

    /* 跳转到 C 入口 */
    bl kernel_start

    /* 不应返回 */
1:  wfi
    b 1b

.section .text
.align 11

/**
 * ARM64 异常向量表
 * 每级 4 个入口（sync, irq, fiq, serror），每个入口 0x80 字节
 * 偏移: 0x000 EL1t sync
 *       0x080 EL1t irq
 *       0x100 EL1t fiq
 *       0x180 EL1t serror
 *       0x200 EL1h sync
 *       0x280 EL1h irq
 *       0x300 EL1h fiq
 *       0x380 EL1h serror
 *       0x400 EL0 sync （64-bit）
 *       0x480 EL0 irq
 *       0x500 EL0 fiq
 *       0x580 EL0 serror
 */
.global exception_vector_table
.type exception_vector_table, @function
exception_vector_table:
    /* EL1t sync */
    b .
    .align 7
    /* EL1t irq */
    b .
    .align 7
    /* EL1t fiq */
    b .
    .align 7
    /* EL1t serror */
    b .
    .align 7

    /* EL1h sync — 用于系统调用/异常 */
    stp x29, x30, [sp, #-16]!
    bl arm64_sync_handler
    ldp x29, x30, [sp], #16
    eret
    .align 7

    /* EL1h irq — 外设中断 */
    stp x29, x30, [sp, #-16]!
    bl arm64_irq_handler
    ldp x29, x30, [sp], #16
    eret
    .align 7

    /* EL1h fiq */
    b .
    .align 7
    /* EL1h serror */
    b .
    .align 7

    /* EL0 sync (64-bit) — SVC 指令入口（保留，IPC 3.0 不使用） */
    b .
    .align 7
    /* EL0 irq */
    b .
    .align 7
    /* EL0 fiq */
    b .
    .align 7
    /* EL0 serror */
    b .
    .align 7
