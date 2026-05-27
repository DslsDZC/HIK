/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * ARM64 上下文切换
 *
 * thread_t->arch_context 布局（由 hal_context_t 定义）：
 *   0:   x19   8:  x20   16: x21   24: x22
 *   32:  x23   40: x24   48: x25   56: x26
 *   64:  x27   72: x28   80: x29   88: SP_EL0
 *   96:  PC    104: SPSR
 */

.section .text
.global arm64_save_context
.global arm64_restore_context
.global arm64_context_init
.global context_switch

/* 保存上下文到 thread_t->arch_context
 * 参数: x0 = arch_context 指针
 */
arm64_save_context:
context_save:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, xzr, [x0, #80]    /* x29 (FP), 暂存 SP_EL0 */

    mov x1, sp
    str x1, [x0, #88]           /* SP_EL0 = current sp */

    mrs x1, ELR_EL1
    str x1, [x0, #96]           /* PC = ELR_EL1 */

    mrs x1, SPSR_EL1
    str x1, [x0, #104]          /* SPSR */

    ret

/* 恢复上下文到 thread_t->arch_context
 * 参数: x0 = arch_context 指针
 */
arm64_restore_context:
context_restore:
    ldp x19, x20, [x0, #0]
    ldp x21, x22, [x0, #16]
    ldp x23, x24, [x0, #32]
    ldp x25, x26, [x0, #48]
    ldp x27, x28, [x0, #64]
    ldp x29, xzr, [x0, #80]

    ldr x1, [x0, #88]
    mov sp, x1

    ldr x1, [x0, #96]
    msr ELR_EL1, x1

    ldr x1, [x0, #104]
    msr SPSR_EL1, x1

    ret

/* 初始化新线程上下文
 * 参数: x0 = arch_context, x1 = entry, x2 = stack_top
 */
arm64_context_init:
context_init:
    stp xzr, xzr, [x0, #0]     /* 清零 x19-x28 */
    stp xzr, xzr, [x0, #16]
    stp xzr, xzr, [x0, #32]
    stp xzr, xzr, [x0, #48]
    stp xzr, xzr, [x0, #64]

    mov x3, sp
    str x3, [x0, #80]           /* x29 = 当前 FP */

    str x2, [x0, #88]           /* SP = stack_top */

    str x1, [x0, #96]           /* PC = entry */
    mov x3, #0x3C5              /* SPSR: EL1h, all interrupts enabled */
    str x3, [x0, #104]

    ret

/* context_switch(prev, next) — 线程切换
 * x0 = prev thread_t*, x1 = next thread_t*
 */
.align 6
context_switch:
    cbz x0, .L_first_switch

    /* 保存 prev 上下文 */
    add x0, x0, #48             /* thread_t->arch_context 偏移 */
    bl context_save

.L_first_switch:
    /* 恢复 next 上下文 */
    add x1, x1, #48
    mov x0, x1

    /* 切换页表（跨域时） */
    ldr w2, [x1, #-44]          /* thread_t->domain_id */
    bl arm64_switch_pagetable

    b context_restore
