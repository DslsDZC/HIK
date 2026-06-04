/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * STM32F103 (Cortex-M3) 线程栈初始化
 *
 * 覆盖 thread.c 中的弱符号 arch_thread_setup_stack()。
 * 为新线程的栈建立 PendSV 返回帧，用于首次上下文切换。
 *
 * 栈布局（顶部→底部）：
 *   [xPSR=0x01000000]    <- CPU 异常返回时自动加载
 *   [PC = entry_point]
 *   [LR = thread_exit_handler]
 *   [R12 = 0]
 *   [R3-R0 = 0]
 *   [R11-R4 = 0]          <- PendSV 手动保存（8 个寄存器）
 *   stack_ptr 指向这里
 *
 * 首次 context_switch 流程：
 *   1. PendSV 触发 → 保存当前线程的 R4-R11 和 SP
 *   2. PendSV 加载新线程的 SP（指向 R4 位置）
 *   3. PendSV 弹出 R4-R11 → SP 指向 [xPSR, PC, LR, R12, R3-R0]
 *   4. PendSV 异常返回（bx lr）→ CPU 硬件弹出 R0-R3,R12,LR,PC,xPSR
 *   5. CPU 跳到 entry_point，LR = thread_exit_handler
 */

#include "types.h"
#include "hal.h"

/* 汇编实现的栈初始化函数 */
extern uint32_t stm32f103_thread_stack_init(uint32_t stack_top,
                                             uint32_t entry_point,
                                             uint32_t exit_handler);

virt_addr_t arch_thread_setup_stack(virt_addr_t stack_top,
                                    virt_addr_t entry_point,
                                    void *thread_exit_handler)
{
    return (virt_addr_t)(uintptr_t)
        stm32f103_thread_stack_init((uint32_t)(uintptr_t)stack_top,
                                    (uint32_t)(uintptr_t)entry_point,
                                    (uint32_t)(uintptr_t)thread_exit_handler);
}
