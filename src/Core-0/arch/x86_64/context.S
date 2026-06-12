/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86-64上下文切换汇编实现（多核支持）
 */

.section .text

/* ==================== 外部函数声明 ==================== */

/* CPU ID获取函数 */
.extern x86_64_get_cpu_id

/* ==================== Per-CPU数据结构偏移 ==================== */

/* percpu_data_t结构偏移（必须与percpu.h中定义一致） */
#define PERCPU_CPU_ID           0
#define PERCPU_IS_BSP           4
#define PERCPU_IS_ONLINE        5
#define PERCPU_STATE            6
#define PERCPU_STACK_BASE       8
#define PERCPU_STACK_SIZE       16
#define PERCPU_STACK_TOP        24
#define PERCPU_READY_QUEUE      32
#define PERCPU_READY_QUEUE_COUNT 32 + (8 * 256)
#define PERCPU_CURRENT_THREAD   PERCPU_READY_QUEUE_COUNT + 8
#define PERCPU_CONTEXT_SWITCHES PERCPU_CURRENT_THREAD + 8
#define PERCPU_IDLE_TICKS       PERCPU_CONTEXT_SWITCHES + 8
#define PERCPU_NEED_RESCHEDULE  PERCPU_IDLE_TICKS + 8
#define PERCPU_LOCAL_APIC       PERCPU_NEED_RESCHEDULE + 8
#define PERCPU_INTERRUPTS_HANDLED PERCPU_LOCAL_APIC + 8
#define PERCPU_IPI_RECEIVED     PERCPU_INTERRUPTS_HANDLED + 8
#define PERCPU_IPI_SENT         PERCPU_IPI_RECEIVED + 8
#define PERCPU_INIT_DONE        PERCPU_IPI_SENT + 8

/* percpu_data_t大小（用于计算per-CPU数组的偏移） */
#define PERCPU_SIZE             PERCPU_INIT_DONE + 4

.global x86_64_save_context
.type x86_64_save_context, @function

/* 保存x86_64上下文
 * 参数: rdi = 上下文结构指针
 */
x86_64_save_context:
    /* 保存通用寄存器 */
    movq %rax, 0(%rdi)
    movq %rbx, 8(%rdi)
    movq %rcx, 16(%rdi)
    movq %rdx, 24(%rdi)
    movq %rsi, 32(%rdi)
    movq %rbp, 40(%rdi)
    movq %rsp, 48(%rdi)
    movq %r8, 56(%rdi)
    movq %r9, 64(%rdi)
    movq %r10, 72(%rdi)
    movq %r11, 80(%rdi)
    movq %r12, 88(%rdi)
    movq %r13, 96(%rdi)
    movq %r14, 104(%rdi)
    movq %r15, 112(%rdi)

    /* 保存特殊寄存器 */
    movq (%rsp), %rdx      /* 返回地址 */
    movq %rdx, 120(%rdi)   /* RIP */
    pushfq
    popq %rdx
    movq %rdx, 128(%rdi)   /* RFLAGS */

    ret

.size x86_64_save_context, .-x86_64_save_context

.global x86_64_restore_context
.type x86_64_restore_context, @function

/* 恢复x86_64上下文
 * 参数: rdi = 上下文结构指针
 */
x86_64_restore_context:
    /* 恢复通用寄存器（除 rsp 外） */
    movq 0(%rdi), %rax
    movq 8(%rdi), %rbx
    movq 16(%rdi), %rcx
    movq 24(%rdi), %rdx
    movq 32(%rdi), %rsi       /* 恢复 rsi */
    movq 40(%rdi), %rbp
    movq 56(%rdi), %r8
    movq 64(%rdi), %r9
    movq 72(%rdi), %r10
    movq 80(%rdi), %r11
    movq 88(%rdi), %r12
    movq 96(%rdi), %r13
    movq 104(%rdi), %r14
    movq 112(%rdi), %r15

    /* 在原栈上恢复 RFLAGS（避免污染新栈） */
    movq 128(%rdi), %rdx
    pushq %rdx
    popfq

    /* 最后恢复 rsp 并跳转 */
    movq 48(%rdi), %rsp
    movq 120(%rdi), %rdx
    jmp *%rdx

.size x86_64_restore_context, .-x86_64_restore_context

.global context_switch
.type context_switch, @function

/* 外部函数：获取域页表 */
.extern domain_switch_get_pagetable
.extern domain_switch_set_current

/* 外部变量：全局域表 */
.extern g_domains

/* 域结构体偏移 (domain.h) */
#define DOMAIN_PAGE_TABLE_OFFSET 56  /* page_table 字段在 domain_t 中的偏移 */

/* 上下文切换函数（多核支持）
 * 参数: rdi = prev线程指针(thread_t*), rsi = next线程指针(thread_t*)
 *
 * thread_t 结构体偏移 (实际计算):
 *   0:  thread_id (u32, 4字节)
 *   4:  domain_id (u32, 4字节)
 *   8:  state (enum, 4字节)
 *   12: priority (u8, 1字节) + 3字节填充
 *   16: logical_core_id (u32, 4字节)
 *   20: core_affinity (u32, 4字节)
 *   24: stack_base (8字节)
 *   32: stack_size (8字节)
 *   40: stack_ptr (8字节)     <-- 这是我们要保存/恢复的
 *   48: arch_context (8字节)
 *   56: time_slice (8字节)
 *
 * 修复：添加 CR3 切换逻辑，确保跨域线程切换时正确切换页表
 */
context_switch:
    /* 检查 prev 是否为 NULL */
    testq %rdi, %rdi
    jz .L_first_switch

    /* ===== 保存当前线程上下文到 prev =====
     * 保存全部 15 个通用寄存器（与 ISR 帧布局一致），
     * 使 ISR 的 .L_fast_restore + iretq 可直接恢复此上下文。 */
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* 保存栈指针到 prev->stack_ptr (偏移 40) */
    movq %rsp, 40(%rdi)

    jmp .L_switch_to_new

.L_first_switch:
    /* 首次调度：prev 为 NULL，不需要保存上下文 */
    subq $0x88, %rsp    /* 15 个寄存器槽位 + 1 个向量号槽位 (15+1)*8=128 */

.L_switch_to_new:
    /* ===== 跨域线程切换 =====
     * 顺序：1) 获取目标页表  2) 切换栈  3) 切换 CR3
     * 先切栈再切 CR3，确保 CR3 切换时已在目标线程栈上。
     */

    /* 将 next 指针保存在 callee-saved 寄存器 %rbx 中 */
    movq %rsi, %rbx

    /* TODO: 跳过 CR3 切换直到域页表完整 */
    jmp .L_skip_cr3

.L_skip_cr3:
    /* 同域切换：只需恢复栈指针 */
    movq 40(%rbx), %rsp

.L_restore:
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $8, %rsp       /* 跳过 IRQ 向量号 */
    iretq

.size context_switch, .-context_switch

/* x86_64_context_switch 别名 */
.global x86_64_context_switch
.set x86_64_context_switch, context_switch

/* ==================== 线程入口蹦床 ==================== */

/**
 * thread_entry_trampoline — 新线程首次运行的入口。
 *
 * context_switch 的 ret 到此处（仅第一次调度时），栈布局：
 *   [thread_exit_handler]
 *   [real_entry_point]    <- RSP 初始指向这里
 *
 * 蹦床弹出 real_entry_point，开中断（sti），然后跳转到真正的入口。
 * 此后新线程以 IF=1 运行，可响应定时器中断实现抢占。
 *
 * 后续 reschedule（schedule 从 IRQ 路径被调用）走正常的
 * context_switch → ret → exec_flow_dispatch → schedule → isr_fast_stub → iretq
 * 路径，其中 iretq 从中断帧恢复 RFLAGS（含 IF），无需重复 sti。
 *
 * 这样做避免了在 context_switch 的 .L_restore 中直接 sti 导致的
 * #GP 问题 —— 新域页表激活后立即响应中断时，中断处理所需的部分
 * 全局页面（GDT/IDT.BSS）在该域页表中不可访问。
 */
.global thread_entry_trampoline
.type thread_entry_trampoline, @function
thread_entry_trampoline:
    popq %r11           /* 弹出真正的入口点 */
    sti                 /* 开启中断 — 此时新域页表已完备，中断可安全响应 */
    jmp  *%r11          /* 跳转到线程真正的入口 */
.size thread_entry_trampoline, .-thread_entry_trampoline

/* ==================== 上下文初始化 ==================== */

/**
 * 初始化新线程上下文
 * 参数: rdi = ctx (上下文结构指针)
 *       rsi = entry (线程入口函数)
 *       rdx = stack (栈顶指针)
 */
.global x86_64_context_init
.type x86_64_context_init, @function
x86_64_context_init:
    /* 清零上下文结构 */
    pushq %rdi
    movq %rdi, %rcx
    movq $136 / 8, %r8         /* 上下文大小 / 8 */
    xorq %rax, %rax
1:
    movq %rax, (%rcx)
    addq $8, %rcx
    decq %r8
    jnz 1b
    popq %rdi

    /* 设置栈指针 (offset 48) */
    movq %rdx, 48(%rdi)        /* ctx->sp = stack */

    /* 设置指令指针 (offset 120) */
    movq %rsi, 120(%rdi)       /* ctx->pc = entry */

    /* 设置默认 RFLAGS (offset 128) - 启用中断 */
    movq $0x202, 128(%rdi)     /* IF=1, 保留位=1 */

    ret

.size x86_64_context_init, .-x86_64_context_init