/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * STM32F103 (Cortex-M3) 内核入口与异常向量表
 *
 * 开机流程：
 *   1. CPU 从 Flash 起始地址加载 SP 和 PC
 *   2. reset_handler 初始化硬件、BSS、C 运行时
 *   3. 跳转到 kernel_start()
 *   4. HalInit → ArchRegister → 系统初始化
 *
 * 向量表：
 *   - 第 0 项：初始 MSP（栈顶）
 *   - 第 1 项：Reset_Handler（入口）
 *   - 后续：各异常/中断入口
 *
 * Cortex-M3 硬件特性：
 *   - 异常入口时 CPU 自动压栈 R0-R3, R12, LR, PC, xPSR（8 寄存器）
 *   - 使用 PendSV (向量 14) 做上下文切换
 *   - 使用 SVCall (向量 11) 做系统调用
 *   - 使用 SysTick (向量 15) 做定时器 tick
 */

.syntax unified
.thumb
.cpu cortex-m3
.fpu softvfp

.section .vectors, "a", %progbits
.type g_vector_table, %object
.global g_vector_table

g_vector_table:
    .word _estack                       /* 0: 初始栈顶 (MSP) */
    .word reset_handler                 /* 1: Reset */
    .word default_exception_handler     /* 2: NMI */
    .word hardfault_handler             /* 3: HardFault */
    .word memmanage_handler             /* 4: MemManage */
    .word busfault_handler              /* 5: BusFault */
    .word usagefault_handler            /* 6: UsageFault */
    .word 0                             /* 7: Reserved */
    .word 0                             /* 8: Reserved */
    .word 0                             /* 9: Reserved */
    .word 0                             /* 10: Reserved */
    .word svcall_handler                /* 11: SVCall */
    .word debug_mon_handler             /* 12: Debug Monitor */
    .word 0                             /* 13: Reserved */
    .word pendsv_handler                /* 14: PendSV */
    .word systick_handler               /* 15: SysTick */

    /* IRQ 0-67 (STM32F103C8T6 有 43 个 IRQ) */
    .word default_irq_handler           /* 16: WWDG */
    .word default_irq_handler           /* 17: PVD */
    .word default_irq_handler           /* 18: TAMPER */
    .word default_irq_handler           /* 19: RTC */
    .word default_irq_handler           /* 20: FLASH */
    .word default_irq_handler           /* 21: RCC */
    .word default_irq_handler           /* 22: EXTI0 */
    .word default_irq_handler           /* 23: EXTI1 */
    .word default_irq_handler           /* 24: EXTI2 */
    .word default_irq_handler           /* 25: EXTI3 */
    .word default_irq_handler           /* 26: EXTI4 */
    .word default_irq_handler           /* 27: DMA1_Channel1 */
    .word default_irq_handler           /* 28: DMA1_Channel2 */
    .word default_irq_handler           /* 29: DMA1_Channel3 */
    .word default_irq_handler           /* 30: DMA1_Channel4 */
    .word default_irq_handler           /* 31: DMA1_Channel5 */
    .word default_irq_handler           /* 32: DMA1_Channel6 */
    .word default_irq_handler           /* 33: DMA1_Channel7 */
    .word default_irq_handler           /* 34: ADC1 */
    .word default_irq_handler           /* 35: USB_HP_CAN1_TX */
    .word default_irq_handler           /* 36: USB_LP_CAN1_RX0 */
    .word default_irq_handler           /* 37: CAN1_RX1 */
    .word default_irq_handler           /* 38: CAN1_SCE */
    .word default_irq_handler           /* 39: EXTI9_5 */
    .word default_irq_handler           /* 40: TIM1_BRK */
    .word default_irq_handler           /* 41: TIM1_UP */
    .word default_irq_handler           /* 42: TIM1_TRG_COM */
    .word default_irq_handler           /* 43: TIM1_CC */
    .word default_irq_handler           /* 44: TIM2 */
    .word default_irq_handler           /* 45: TIM3 */
    .word default_irq_handler           /* 46: TIM4 */
    .word default_irq_handler           /* 47: I2C1_EV */
    .word default_irq_handler           /* 48: I2C1_ER */
    .word default_irq_handler           /* 49: I2C2_EV */
    .word default_irq_handler           /* 50: I2C2_ER */
    .word default_irq_handler           /* 51: SPI1 */
    .word default_irq_handler           /* 52: SPI2 */
    .word default_irq_handler           /* 53: USART1 */
    .word default_irq_handler           /* 54: USART2 */
    .word default_irq_handler           /* 55: USART3 */
    .word default_irq_handler           /* 56: EXTI15_10 */
    .word default_irq_handler           /* 57: RTC_Alarm */
    .word default_irq_handler           /* 58: USBWakeup */
    .word 0                             /* 59: Reserved */
    .word 0                             /* 60: Reserved */
    .word 0                             /* 61: Reserved */
    .word 0                             /* 62: Reserved */
    .word 0                             /* 63: Reserved */
    .word 0                             /* 64: Reserved */
    .word 0                             /* 65: Reserved */
    .word 0                             /* 66: Reserved */
    .word 0                             /* 67: Reserved */
.size g_vector_table, .-g_vector_table

/* ==================== Reset Handler ==================== */

.section .text.reset_handler, "ax", %progbits
.type reset_handler, %function
.global reset_handler

reset_handler:
    /* 设置栈指针 (MSP) */
    ldr r0, =_estack
    msr MSP, r0

    /* 设置 CONTROL 寄存器：特权模式 + MSP */
    mov r0, #0
    msr CONTROL, r0
    isb

    /* 复制 .data 段从 Flash 到 SRAM（内联实现，不依赖外部函数） */
    ldr r0, =_sidata        /* Flash 中 .data 的源地址 */
    ldr r1, =_sdata         /* SRAM 中 .data 的目标地址 */
    ldr r2, =_edata         /* .data 结束地址 */
    subs r2, r2, r1         /* 计算大小 */
    beq 1f                  /* 大小为 0 则跳过 */
    /* 内联 memcpy: r0=src, r1=dst, r2=size */
    .L_copy:
        ldrb r3, [r0], #1
        strb r3, [r1], #1
        subs r2, r2, #1
        bne .L_copy
1:

    /* 清零 BSS 段（内联实现，不依赖外部函数） */
    ldr r0, =_sbss          /* BSS 起始地址 */
    ldr r1, =_ebss          /* BSS 结束地址 */
    subs r1, r1, r0         /* 计算大小 */
    beq 2f                  /* 大小为 0 则跳过 */
    mov r2, #0
    .L_bss_clr:
        strb r2, [r0], #1
        subs r1, r1, #1
        bne .L_bss_clr
2:

    /* 设置系统时钟 (HSE → PLL → 72MHz) */
    bl SystemClock_Init

    /* 初始化 UART（控制台输出） */
    bl stm32f103_uart_early_init

    /* 跳转到 C 入口 kernel_start */
    bl kernel_start

    /* 不应返回；若返回则挂起 */
    .L_hang:
    wfi
    b .L_hang

.size reset_handler, .-reset_handler

/* ==================== 异常处理程序 ==================== */

/* 默认异常处理（无限循环） */
.section .text.default_exception_handler, "ax", %progbits
.type default_exception_handler, %function
.weak default_exception_handler
default_exception_handler:
    /* 保存关键寄存器用于调试 */
    mov r0, lr              /* EXC_RETURN */
    mrs r1, MSP             /* 当前栈指针 */
    mrs r2, PSP             /* 进程栈指针（如果使用） */
    /* 调用 C 崩溃处理 */
    bl panic_default_exception
1:  wfi
    b 1b
.size default_exception_handler, .-default_exception_handler

/* HardFault 处理 */
.section .text.hardfault_handler, "ax", %progbits
.type hardfault_handler, %function
.weak hardfault_handler
hardfault_handler:
    /* Cortex-M3 进入 HardFault 时已自动压栈 R0-R3,R12,LR,PC,xPSR */
    mov r0, lr              /* r0 = EXC_RETURN */
    mrs r1, MSP             /* r1 = MSP (异常时的 SP) */
    /* 根据 EXC_RETURN 判断使用 MSP 还是 PSP */
    mov r2, r1              /* r2 = SP with stack frame */
    tst r0, #4
    ite eq
    mrseq r2, MSP
    mrsne r2, PSP
    /* r2 指向自动压栈的帧 */
    bl panic_hardfault
1:  wfi
    b 1b
.size hardfault_handler, .-hardfault_handler

/* MemManage / BusFault / UsageFault */
.weak memmanage_handler
memmanage_handler:
    b default_exception_handler
.weak busfault_handler
busfault_handler:
    b default_exception_handler
.weak usagefault_handler
usagefault_handler:
    b default_exception_handler
.weak debug_mon_handler
debug_mon_handler:
    b default_exception_handler

/* ==================== PendSV: 上下文切换 ==================== */

.section .text.pendsv_handler, "ax", %progbits
.type pendsv_handler, %function
.weak pendsv_handler
.global pendsv_handler

pendsv_handler:
    /*
     * Cortex-M3 进入 PendSV 时 CPU 已自动压栈：
     *   [SP+0]  = R0
     *   [SP+4]  = R1
     *   [SP+8]  = R2
     *   [SP+12] = R3
     *   [SP+16] = R12
     *   [SP+20] = LR (EXC_RETURN)
     *   [SP+24] = PC (返回地址)
     *   [SP+28] = xPSR
     *
     * 我们需要额外手动压栈：R4-R11
     */

    /* 保存 R4-R11 到当前栈 */
    push {r4, r5, r6, r7, r8, r9, r10, r11}

    /* 获取当前线程的栈指针 */
    ldr r0, =g_current_thread
    ldr r0, [r0]            /* r0 = g_current_thread (thread_t*) */
    cbz r0, .L_pendsv_restore

    /* 保存当前 SP (MSP) 到 thread->stack_ptr (偏移 40) */
    mrs r1, MSP
    str r1, [r0, #40]

.L_pendsv_restore:
    /* 加载 next 线程指针 */
    ldr r1, =g_next_thread_to_switch
    ldr r1, [r1]            /* r1 = g_next_thread_to_switch */
    cbz r1, .L_pendsv_skip

    /* 从 next->stack_ptr (偏移 40) 恢复 SP */
    ldr r2, [r1, #40]
    msr MSP, r2

    /* 更新 g_current_thread */
    ldr r0, =g_current_thread
    str r1, [r0]

    /* 清零 g_next_thread_to_switch */
    ldr r0, =g_next_thread_to_switch
    mov r2, #0
    str r2, [r0]

.L_pendsv_skip:
    /* 恢复 R4-R11 */
    pop {r4, r5, r6, r7, r8, r9, r10, r11}

    /* 异常返回：CPU 自动恢复 R0-R3, R12, LR, PC, xPSR */
    bx lr                   /* LR = EXC_RETURN */
.size pendsv_handler, .-pendsv_handler

/* ==================== SVCall: 系统调用 ==================== */

.section .text.svcall_handler, "ax", %progbits
.type svcall_handler, %function
.weak svcall_handler
.global svcall_handler

svcall_handler:
    /*
     * 系统调用约定（Cortex-M3）：
     *   R0-R3: 参数
     *   R7:    系统调用号（或 R12）
     *
     * CPU 已自动压栈 R0-R3, R12, LR, PC, xPSR
     * 我们可以读取栈上的参数，或直接从寄存器读取。
     */
    push {lr}               /* 保存 EXC_RETURN */

    /* 从自动压栈的帧读取参数 */
    mrs r0, MSP
    /* r0 = SP，指向自动压栈帧：
     *   r0[0] = func_arg0 (R0 at exception entry)
     *   r0[1] = func_arg1 (R1 at exception entry)
     *   r0[2] = func_arg2 (R2 at exception entry)
     *   r0[3] = func_arg3 (R3 at exception entry)
     *   r0[4] = R12
     *   r0[5] = LR
     *   r0[6] = PC
     *   r0[7] = xPSR
     */
    /* 系统调用号在 R7 中，参数在 R0-R3 中 */
    mov r0, r7              /* r0 = syscall_num */
    /* R1-R3 已经是参数 1-3 了 */
    bl syscall_handler_c    /* syscall_handler_c(num, a1, a2, a3) */

    pop {pc}                /* 返回，CPU 恢复自动压栈的寄存器 */
.size svcall_handler, .-svcall_handler

/* ==================== SysTick Handler ==================== */

.section .text.systick_handler, "ax", %progbits
.type systick_handler, %function
.weak systick_handler
.global systick_handler

systick_handler:
    push {lr}               /* 保存 LR (EXC_RETURN) */
    bl scheduler_tick       /* 设置 g_reschedule_needed = 1 */
    bl stm32_irq_eoi        /* 清 SysTick 中断标志 */
    pop {pc}                /* 异常返回 */
.size systick_handler, .-systick_handler

/* ==================== 默认 IRQ 处理 ==================== */

.section .text.default_irq_handler, "ax", %progbits
.type default_irq_handler, %function
.weak default_irq_handler

default_irq_handler:
    /*
     * 默认 IRQ 处理 — 通过 irq_dispatch 分发。
     *
     * Cortex-M3 进入 IRQ handler 时，CPU 已自动压栈了
     * R0-R3, R12, LR(EXC_RETURN), PC, xPSR。
     *
     * 栈布局（MSP 指向自动压栈帧）：
     *   [SP+0]  R0
     *   [SP+4]  R1
     *   [SP+8]  R2
     *   [SP+12] R3
     *   [SP+16] R12
     *   [SP+20] LR (EXC_RETURN)
     *   [SP+24] PC
     *   [SP+28] xPSR
     */
    push {lr}

    /* 获取中断号：读取 ICSR 的 VECTACTIVE 字段 */
    ldr r0, =0xE000ED04    /* SCB_ICSR 地址 */
    ldr r1, [r0]           /* 读取 ICSR */
    ldr r0, =0x1FF         /* VECTACTIVE 掩码 (bits 0-8) */
    and r0, r1, r0         /* r0 = 中断向量号 (16 + IRQn) */
    subs r0, r0, #16       /* 转换为 IRQ 号 (0-67) */

    /* 计算 irq_table 索引 */
    ldr r1, =irq_table
    lsl r2, r0, #4         /* * sizeof(irq_route_entry_t) = 16 */
    add r1, r1, r2
    ldr r2, [r1]           /* handler_address */
    cbz r2, .L_irq_spurious

    /* 调用 handler */
    blx r2

.L_irq_spurious:
    pop {pc}               /* 异常返回 */
.size default_irq_handler, .-default_irq_handler

/* ==================== 全局数据 ==================== */

.section .bss
/* 下一个要切换到的线程（由 schedule() 设置，PendSV 读取） */
.global g_next_thread_to_switch
g_next_thread_to_switch:
    .space 4, 0
