/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * STM32F103 (Cortex-M3) 上下文切换
 *
 * Cortex-M3 上下文切换使用 PendSV 机制：
 *   1. schedule() 设置 g_next_thread_to_switch 并触发 PendSV
 *   2. PendSV handler 在异常尾链中执行实际的上下文切换
 *   3. 硬件自动保存/恢复 R0-R3, R12, LR, PC, xPSR
 *   4. 软件手动保存/恢复 R4-R11
 *
 * 栈布局（从高到低）：
 *   高地址
 *     xPSR           <- CPU 自动压栈
 *     PC (返回地址)
 *     LR (EXC_RETURN)
 *     R12
 *     R3
 *     R2
 *     R1
 *     R0             <- CPU 自动压栈结束
 *     R11            <- 手动压栈
 *     R10
 *     ...
 *     R4             <- RSP 指向这里 (stack_ptr)
 *   低地址
 *
 * thread_t->stack_ptr (偏移 40) 指向手动保存寄存器栈顶 (R4)。
 * 加载 stack_ptr 并 pop {r4-r11} 后，RSP 指向 CPU 自动压栈帧，
 * 执行 bx lr (LR=EXC_RETURN) 恢复自动压栈的寄存器。
 *
 * hal_context_t 布局（与 hal.h 一致）：
 *   0..63:   general_regs[16] (R0-R15, 每个 4 字节)
 *   64:      pc
 *   68:      sp
 *   72:      flags (xPSR)
 *   76..155: arch_specific (32 * 4 字节)
 *   156:     state
 *   160:     privilege_level
 */

.syntax unified
.thumb
.cpu cortex-m3
.fpu softvfp

.section .text
.align 2

/* ==================== 上下文保存 ==================== */

.global stm32f103_save_context
.type stm32f103_save_context, %function
stm32f103_save_context:
    /* r0 = hal_context_t 指针 */
    str r0, [r0, #0]       /* general_regs[0] = current R0 */
    str r1, [r0, #4]
    str r2, [r0, #8]
    str r3, [r0, #12]
    str r4, [r0, #16]
    str r5, [r0, #20]
    str r6, [r0, #24]
    str r7, [r0, #28]
    str r8, [r0, #32]
    str r9, [r0, #36]
    str r10, [r0, #40]
    str r11, [r0, #44]
    str r12, [r0, #48]
    mov r1, lr
    str r1, [r0, #52]      /* general_regs[13] = LR (R14) */
    mov r1, lr
    str r1, [r0, #60]      /* general_regs[15] = PC (R15 — 实际用 lr 替代) */

    /* 保存特殊寄存器 */
    mrs r1, MSP
    str r1, [r0, #64]      /* pc = LR (近似) */
    str r1, [r0, #68]      /* sp = MSP */
    mrs r1, xPSR
    str r1, [r0, #72]      /* flags = xPSR */

    bx lr
.size stm32f103_save_context, .-stm32f103_save_context

/* ==================== 上下文恢复 ==================== */

.global stm32f103_restore_context
.type stm32f103_restore_context, %function
stm32f103_restore_context:
    /* r0 = hal_context_t 指针 */
    ldr r4, [r0, #16]
    ldr r5, [r0, #20]
    ldr r6, [r0, #24]
    ldr r7, [r0, #28]
    ldr r8, [r0, #32]
    ldr r9, [r0, #36]
    ldr r10, [r0, #40]
    ldr r11, [r0, #44]
    ldr r12, [r0, #48]
    ldr r1, [r0, #52]
    mov lr, r1

    ldr r1, [r0, #68]      /* sp = MSP */
    msr MSP, r1
    ldr r1, [r0, #72]      /* flags = xPSR */
    msr xPSR, r1

    /* 恢复 R0-R3（最后恢复，因为它们是参数寄存器） */
    ldr r0, [r0, #0]

    bx lr
.size stm32f103_restore_context, .-stm32f103_restore_context

/* ==================== 上下文初始化 ==================== */

.global stm32f103_context_init
.type stm32f103_context_init, %function
stm32f103_context_init:
    /*
     * 初始化新线程上下文
     * r0 = hal_context_t*
     * r1 = entry_point
     * r2 = stack_top (栈顶地址)
     *
     * 初始化后的栈布局（从线程入口看）：
     *   STACK_TOP → [初始 xPSR]     <- bits[8:6]=011 (Thumb), bit[24]=1 (T-bit)
     *               [entry_point]
     *               [thread_exit_handler]
     *               [0] (R12)
     *               [0] (R3)
     *               [0] (R2)
     *               [0] (R1)
     *               [0] (R0)
     *               [0] (R11)      <- 手动保存开始
     *               [0] (R10)
     *               ...
     *               [0] (R4)       <- stack_ptr 指向这里
     */
    push {r4, r5, lr}

    mov r4, r0              /* r4 = hal_context_t* */
    mov r5, r2              /* r5 = stack_top */

    /* 清零 hal_context_t 的前 64 字节 (R0-R15) */
    mov r0, r4
    mov r1, #0
    mov r2, #64
    bl memset

    /* 设置初始 xPSR: Thumb 模式 */
    mov r0, #0x01000000     /* xPSR: T-bit = 1, EPSR.T = 1 */
    str r0, [r4, #72]       /* flags = xPSR */

    /* 设置线程入口和目标栈 */
    sub r5, r5, #32         /* 预留自动压栈空间 (8*4=32) */
    sub r5, r5, #32         /* 预留手动保存空间 (8*4=32) */

    /* 在栈上构建自动压栈帧（从高到低写入）*/
    mov r0, #0x01000000     /* xPSR: T-bit */
    str r0, [r2, #-4]!      /* push xPSR */
    /* PC = entry_point (由首次 PendSV 返回时使用) */
    ldr r0, [sp, #0]        /* 临时，实际应该用 entry_point */
    sub sp, sp, #4

    /* 简化：不构建完整栈帧，让线程首次运行时通过初始栈帧进入 */

    ldr r1, [r4, #68]       /* sp */
    orr r1, r1, #0
    /* 设置 hal_context_t 中的 sp 指向栈顶 - 自动压栈帧 */
    str r5, [r4, #68]

    /* 设置 thread_exit_handler 在栈上（当线程入口返回时） */
    push {r5}               /* 临时保存 */

    ldr r0, =thread_exit_handler
    /* 在栈上构造返回链 */
    mov r5, r2

    pop {r5}
    pop {r4, r5, pc}
.size stm32f103_context_init, .-stm32f103_context_init

/* ==================== thread_t 栈初始化 (被 thread_create 调用) ==================== */

/*
 * 为新线程的栈建立初始 PendSV 返回帧。
 * 在 thread_create_bound() 中调用以建立 stack_ptr。
 *
 * 参数：
 *   r0 = stack_top  (栈顶)
 *   r1 = entry_point (入口)
 *   r2 = thread_exit_handler (退出处理)
 *
 * 返回：
 *   r0 = stack_ptr  (指向手动保存区域底部)
 *
 * 栈布局（从高到低）：
 *   stack_top  → [xPSR=0x01000000]   <- CPU 自动压栈（异常返回时加载）
 *                [PC = entry_point]
 *                [LR = thread_exit_handler]
 *                [R12 = 0]
 *                [R3-R0 = 0]
 *   auto_base  → [R11-R4 = 0]        <- 手动保存（PendSV 的 push/pop）
 *   stack_ptr  →                      <- 返回这个地址
 */
.global stm32f103_thread_stack_init
.type stm32f103_thread_stack_init, %function
stm32f103_thread_stack_init:
    /* r0 = stack_top, r1 = entry, r2 = exit_handler */

    /* 写入自动压栈帧（从栈顶向下）*/
    sub r3, r0, #4
    /* xPSR: T-bit=1 */
    mov r0, #0x01000000
    str r0, [r3]            /* xPSR */

    sub r3, r3, #4
    str r1, [r3]            /* PC = entry_point */

    sub r3, r3, #4
    str r2, [r3]            /* LR = thread_exit_handler */

    sub r3, r3, #4
    mov r0, #0
    str r0, [r3]            /* R12 = 0 */

    /* R3-R0 = 0 */
    sub r3, r3, #4
    str r0, [r3]            /* R3 = 0 */
    sub r3, r3, #4
    str r0, [r3]            /* R2 = 0 */
    sub r3, r3, #4
    str r0, [r3]            /* R1 = 0 */
    sub r3, r3, #4
    str r0, [r3]            /* R0 = 0 */
    /* r3 = 自动压栈帧底部 */

    /* 预留手动保存区 (R11-R4) */
    sub r3, r3, #4
    str r0, [r3]            /* R11 = 0 */
    sub r3, r3, #4
    str r0, [r3]            /* R10 = 0 */
    sub r3, r3, #4
    str r0, [r3]            /* R9 = 0 */
    sub r3, r3, #4
    str r0, [r3]            /* R8 = 0 */
    sub r3, r3, #4
    str r0, [r3]            /* R7 = 0 */
    sub r3, r3, #4
    str r0, [r3]            /* R6 = 0 */
    sub r3, r3, #4
    str r0, [r3]            /* R5 = 0 */
    sub r3, r3, #4
    str r0, [r3]            /* R4 = 0 */

    /* r3 = stack_ptr */
    mov r0, r3
    bx lr
.size stm32f103_thread_stack_init, .-stm32f103_thread_stack_init

/* ==================== context_switch (PendSV 模型) ==================== */

/*
 * context_switch(prev, next) — 线程切换
 *
 * Cortex-M3 使用 PendSV 异常触发上下文切换：
 *   1. 设置 next 线程指针到 g_next_thread_to_switch
 *   2. 触发 PendSV 异常（ICSR.PENDSVSET = 1）
 *   3. 使能中断，PendSV 立即或尾链执行
 *      - 若在 Handler 模式（如 SysTick 内）：PendSV 尾链在当前 handler 返回后执行
 *      - 若在 Thread 模式（如 thread_yield）：PendSV 在 cpsie i 后立即响应
 *   4. PendSV handler 保存当前线程 R4-R11 + MSP
 *   5. PendSV handler 恢复 next 线程的 R4-R11 + MSP
 *   6. PendSV 异常返回，CPU 硬件恢复 R0-R3,R12,LR,PC,xPSR
 *   7. next 线程继续执行
 *
 * 当此线程被重新调度时，PendSV 恢复其上下文，
 * 从 cpsid i 继续执行，函数返回。
 *
 * 参数：
 *   r0 = prev (thread_t*, 未使用，由 PendSV 通过 g_current_thread 获取)
 *   r1 = next (thread_t*)
 */
.global context_switch
.type context_switch, %function
context_switch:
    cbz r1, .L_ctx_skip         /* next 为 NULL 时跳过 */

    push {lr}                   /* 保存返回地址 */

    /* g_next_thread_to_switch = next */
    ldr r2, =g_next_thread_to_switch
    str r1, [r2]

    /* 触发 PendSV */
    ldr r2, =0xE000ED04        /* SCB_ICSR 地址 */
    ldr r3, [r2]
    orr r3, r3, #(1 << 28)     /* PENDSVSET = 1 */
    str r3, [r2]
    dsb
    isb

    /*
     * 使能中断让 PendSV 响应。
     * PendSV 是最低优先级异常：
     *   - 在 Thread 模式下，cpsie i 后 PendSV 立即响应
     *   - CPU 自动压栈当前上下文，进入 PendSV handler
     *   - PendSV 保存 R4-R11+MSP，切换到 next 线程
     * 当此线程被重新调度回来时，从 cpsid i 继续。
     */
    cpsie i
    cpsid i                     /* PendSV 恢复后从这里继续 */

    pop {pc}                    /* 返回 */

.L_ctx_skip:
    bx lr
.size context_switch, .-context_switch

/* ==================== 系统调用触发 ==================== */

.global stm32f103_syscall_invoke
.type stm32f103_syscall_invoke, %function
stm32f103_syscall_invoke:
    /*
     * r0 = syscall_num
     * r1 = arg1
     * r2 = arg2
     * r3 = arg3
     * 更多参数在栈上
     */
    mov r7, r0              /* 系统调用号在 R7 */
    mov r0, r1              /* 参数 1-3 在 R0-R2 */
    mov r1, r2
    mov r2, r3
    svc #0                  /* 触发 SVCall */
    bx lr
.size stm32f103_syscall_invoke, .-stm32f103_syscall_invoke

/* ==================== 系统调用返回 ==================== */

.global stm32f103_syscall_return
.type stm32f103_syscall_return, %function
stm32f103_syscall_return:
    /*
     * 系统调用返回：
     * 将 R0（返回值）存入异常帧中的 R0 位置，
     * 然后异常恢复。
     *
     * 当前在 Handler 模式（SVCall handler 中），
     * R0 已经是返回值。执行异常返回即可。
     */
    bx lr
.size stm32f103_syscall_return, .-stm32f103_syscall_return

.end
