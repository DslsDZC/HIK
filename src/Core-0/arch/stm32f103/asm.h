/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * STM32F103 (Cortex-M3) 汇编常量与宏
 */

#ifndef ARCH_STM32F103_ASM_H
#define ARCH_STM32F103_ASM_H

/* ==================== 特殊寄存器 ==================== */

/* 异常返回标志 (EXC_RETURN) */
#define EXC_RETURN_THREAD_MSP    0xFFFFFFF9  /* 返回线程模式，使用 MSP */
#define EXC_RETURN_THREAD_PSP    0xFFFFFFFD  /* 返回线程模式，使用 PSP */
#define EXC_RETURN_HANDLER       0xFFFFFFF1  /* 返回 Handler 模式（异常嵌套） */

/* CONTROL 寄存器位 */
#define CONTROL_FPCA             (1 << 2)   /* 浮点上下文自动保存 */
#define CONTROL_SPSEL            (1 << 1)   /* 栈指针选择：0=MSP, 1=PSP */
#define CONTROL_NPRIV            (1 << 0)   /* 非特权模式 */

/* ==================== 系统异常号 ==================== */
#define EXC_THREAD_MODE          0
#define EXC_RESET                1
#define EXC_NMI                  2
#define EXC_HARDFAULT            3
#define EXC_MEMMANAGE            4
#define EXC_BUSFAULT             5
#define EXC_USAGEFAULT           6
#define EXC_RESERVED             7
#define EXC_SVCALL              11
#define EXC_DEBUG_MONITOR       12
#define EXC_PENDSV              14
#define EXC_SYSTICK             15

/* ==================== 系统控制块 (SCB) ==================== */
#define SCB_BASE                0xE000ED00
#define SCB_ICSR                (SCB_BASE + 0x04)   /* 中断控制与状态寄存器 */
#define SCB_VTOR                (SCB_BASE + 0x08)   /* 向量表偏移寄存器 */

/* ICSR 位 */
#define ICSR_PENDSVSET          (1 << 28)           /* PendSV 触发位 */
#define ICSR_PENDSVCLR          (1 << 27)           /* PendSV 清除位 */

/* ==================== 外设基址 ==================== */
#define NVIC_BASE               0xE000E100
#define SYSTICK_BASE            0xE000E010

/* ==================== 线程状态偏移 ==================== */

/*
 * thread_t 结构体偏移（与 thread.h 一致）：
 *   0:   thread_id        (u32, 4字节)
 *   4:   domain_id        (u32, 4字节)
 *   8:   state            (enum, 4字节)
 *   12:  priority         (u8, 1字节) + 3 字节填充
 *   16:  logical_core_id  (u32, 4字节)
 *   20:  core_affinity    (u32, 4字节)
 *   24:  stack_base       (8字节)
 *   32:  stack_size       (8字节)
 *   40:  stack_ptr        (8字节)   <-- 保存/恢复的栈指针
 *   48:  arch_context     (8字节)   <-- 指向 hal_context_t
 */

/* ==================== USART1 ==================== */
#define USART1_BASE             0x40013800
#define USART_SR                0x00    /* 状态寄存器 */
#define USART_DR                0x04    /* 数据寄存器 */
#define USART_BRR               0x08    /* 波特率寄存器 */
#define USART_CR1               0x0C    /* 控制寄存器 1 */
#define USART_CR2               0x10    /* 控制寄存器 2 */
#define USART_CR3               0x14    /* 控制寄存器 3 */

#define USART_SR_TXE            (1 << 7)  /* 发送数据寄存器空 */
#define USART_SR_RXNE           (1 << 5)  /* 接收数据寄存器非空 */
#define USART_CR1_UE            (1 << 13) /* USART 使能 */
#define USART_CR1_TE            (1 << 3)  /* 发送使能 */
#define USART_CR1_RE            (1 << 2)  /* 接收使能 */

/* ==================== RCC（复位和时钟控制） ==================== */
#define RCC_BASE                0x40021000
#define RCC_APB2ENR             (RCC_BASE + 0x18)

#define RCC_APB2ENR_USART1EN    (1 << 14)  /* USART1 时钟使能 */
#define RCC_APB2ENR_IOPAEN      (1 << 2)   /* GPIOA 时钟使能 */

/* ==================== GPIOA ==================== */
#define GPIOA_BASE              0x40010800
#define GPIO_CRH                0x04       /* 配置寄存器高位 */
#define GPIO_BSRR               0x10       /* 置位/复位寄存器 */

/* ==================== 栈帧偏移（Cortex-M3 自动压栈） ==================== */
#define STACK_R0                0
#define STACK_R1                4
#define STACK_R2                8
#define STACK_R3                12
#define STACK_R12               16
#define STACK_LR                20          /* 异常返回的 LR = EXC_RETURN */
#define STACK_PC                24          /* 返回地址 */
#define STACK_xPSR              28          /* 程序状态寄存器 */

/* 完整栈帧大小（CPU 自动压栈 8 个寄存器 = 32 字节） */
#define STACK_FRAME_SIZE        32

/* 手动保存的寄存器 (在自动压栈之上) */
#define STACK_R4                0
#define STACK_R5                4
#define STACK_R6                8
#define STACK_R7                12
#define STACK_R8                16
#define STACK_R9                20
#define STACK_R10               24
#define STACK_R11               28

#define STACK_EXTRA_SIZE        32      /* R4-R11 = 8 * 4 = 32 字节 */

#endif /* ARCH_STM32F103_ASM_H */
