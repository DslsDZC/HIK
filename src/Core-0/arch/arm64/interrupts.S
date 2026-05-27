/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * ARM64 异常/中断处理
 */

.section .text

.macro save_full_context
    sub sp, sp, #288
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    mov x0, sp
    add x0, x0, #288
    str x0, [sp, #240]      /* SP_EL0 (original sp before save) */
    mrs x0, ELR_EL1
    str x0, [sp, #248]      /* ELR_EL1 */
    mrs x0, SPSR_EL1
    str x0, [sp, #256]      /* SPSR_EL1 */
    mrs x0, ESR_EL1
    str x0, [sp, #264]      /* ESR_EL1 */
.endm

.macro restore_full_context
    ldp x0, x1, [sp, #256]
    msr SPSR_EL1, x0
    msr ELR_EL1, x1
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    add sp, sp, #288
.endm

.align 6
.global arm64_sync_handler
.type arm64_sync_handler, @function
arm64_sync_handler:
    save_full_context
    mov x0, sp
    mrs x1, ESR_EL1
    lsr x1, x1, #26
    bl arm64_exception_dispatch
    restore_full_context
    eret

.align 6
.global arm64_irq_handler
.type arm64_irq_handler, @function
arm64_irq_handler:
    save_full_context
    mov x0, sp
    bl arm64_irq_dispatch
    restore_full_context
    eret

/* ipc3_return_asm — 恢复 ipc3_saved_context_t 并返回调用者 */
.global ipc3_return_asm
.type ipc3_return_asm, @function
ipc3_return_asm:
    msr daifset, #2

    adrp x0, g_ipc3_return_ctx
    ldr x0, [x0, :lo12:g_ipc3_return_ctx]

    /* 恢复 cr3 (offset 160) → TTBR0 */
    ldr x1, [x0, #160]
    cbz x1, 1f
    msr ttbr0_el1, x1
    isb
1:
    /* 恢复通用寄存器 */
    ldp x1, x2, [x0, #0]     /* r15/r14 → x1,x2 */
    ldp x3, x4, [x0, #16]    /* r13/r12 → x3,x4 */
    ldp x5, x6, [x0, #32]    /* r11/r10 → x5,x6 */
    ldp x7, x8, [x0, #48]    /* r9/r8  → x7,x8 */
    ldr x9, [x0, #64]        /* rbp → x29 */
    mov x29, x9
    ldp x9, x10, [x0, #72]   /* rdi/rsi → x9,x10 */
    ldp x11, x12, [x0, #88]  /* rdx/rcx → x11,x12 */
    ldr x13, [x0, #104]      /* rbx → x13 */
    ldr x14, [x0, #112]      /* rax → x14 */

    /* 恢复 SP (offset 144) 和 ELR (offset 120) */
    ldr x15, [x0, #144]      /* rsp field */
    mov sp, x15
    ldr x15, [x0, #120]      /* rip field = ELR_EL1 */
    msr elr_el1, x15
    ldr x15, [x0, #136]      /* rflags field = SPSR_EL1 */
    msr spsr_el1, x15

    eret
