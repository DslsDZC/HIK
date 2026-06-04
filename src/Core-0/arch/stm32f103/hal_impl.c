/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * STM32F103 (Cortex-M3) 架构特定 HAL 实现
 *
 * MCU: STM32F103C8T6
 * Core: ARM Cortex-M3 @ 72MHz
 * Flash: 64KB (0x08000000 - 0x0800FFFF)
 * SRAM:  20KB (0x20000000 - 0x20004FFF)
 *
 * 特性：
 * - 无 MMU（使用 MPU 做内存保护）
 * - 无 IO 端口（仅有内存映射 IO）
 * - 单核（无 SMP）
 * - 所有代码运行在 Handler 模式（特权级）
 * - SysTick 提供系统 tick
 * - PendSV 做上下文切换
 * - SVCall 做系统调用
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hal.h"
#include "irq.h"
#include "lib/console.h"

/* ==================== 外部汇编函数 ==================== */

extern void stm32f103_save_context(void *ctx);
extern void stm32f103_restore_context(void *ctx);
extern void stm32f103_context_init(void *ctx, void *entry, void *stack);
extern void stm32f103_trigger_pendsv(void);
extern void stm32f103_syscall_invoke(uint64_t num, uint64_t a1, uint64_t a2,
                                     uint64_t a3, uint64_t a4);

/* context_switch 用 PendSV 实现 */
extern void context_switch(void *prev, void *next);

/* ==================== 系统时钟初始化 ==================== */

/* STM32F103 寄存器定义 */
#define RCC_BASE            0x40021000UL
#define RCC_CR              (*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_CFGR            (*(volatile uint32_t *)(RCC_BASE + 0x04))
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x18))

/* RCC_CR 位 */
#define RCC_CR_HSION        (1 << 0)
#define RCC_CR_HSIRDY       (1 << 1)
#define RCC_CR_HSEON        (1 << 16)
#define RCC_CR_HSERDY       (1 << 17)
#define RCC_CR_PLLON        (1 << 24)
#define RCC_CR_PLLRDY       (1 << 25)

/* RCC_CFGR 位 */
#define RCC_CFGR_SW_HSI     0x0
#define RCC_CFGR_SW_HSE     0x1
#define RCC_CFGR_SW_PLL     0x2
#define RCC_CFGR_SWS_PLL    (0x2 << 2)
#define RCC_CFGR_PLLSRC     (1 << 16)       /* PLL 时钟源：HSE */
#define RCC_CFGR_PLLXTPRE   (1 << 17)       /* HSE 分频 */
#define RCC_CFGR_PLLMUL_9   (7 << 18)       /* PLL 倍频 x9（8MHz * 9 = 72MHz） */
#define RCC_CFGR_HPRE_DIV1  (0 << 4)        /* AHB 预分频 = /1 */
#define RCC_CFGR_PPRE2_DIV1 (0 << 11)       /* APB2 预分频 = /1 */
#define RCC_CFGR_PPRE1_DIV2 (4 << 8)        /* APB1 预分频 = /2（最大 36MHz） */

/* FLASH */
#define FLASH_ACR           (*(volatile uint32_t *)0x40022000)
#define FLASH_ACR_LATENCY_2 (2 << 0)        /* 2 等待周期 @72MHz */

/* ==================== USART1 ==================== */

#define USART1_BASE         0x40013800UL
#define USART1_SR           (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define USART1_DR           (*(volatile uint32_t *)(USART1_BASE + 0x04))
#define USART1_BRR          (*(volatile uint32_t *)(USART1_BASE + 0x08))
#define USART1_CR1          (*(volatile uint32_t *)(USART1_BASE + 0x0C))

#define USART_SR_TXE        (1 << 7)
#define USART_SR_RXNE       (1 << 5)
#define USART_CR1_UE        (1 << 13)
#define USART_CR1_TE        (1 << 3)
#define USART_CR1_RE        (1 << 2)

/* ==================== GPIOA (USART1 TX=PA9, RX=PA10) ==================== */

#define GPIOA_BASE          0x40010800UL
#define GPIOA_CRH           (*(volatile uint32_t *)(GPIOA_BASE + 0x04))
#define GPIOA_BSRR          (*(volatile uint32_t *)(GPIOA_BASE + 0x10))

/* ==================== SystemClock_Init ==================== */

void SystemClock_Init(void)
{
    /* 使能 HSE 外部晶振（8MHz） */
    RCC_CR |= RCC_CR_HSEON;
    { volatile int i = 0; while (!(RCC_CR & RCC_CR_HSERDY) && i < 1000000) i++; }

    /* 配置 Flash 等待周期 */
    FLASH_ACR = FLASH_ACR_LATENCY_2;

    /* 配置 AHB/APB 预分频 */
    RCC_CFGR = RCC_CFGR_HPRE_DIV1   /* AHB = SYSCLK / 1 = 72MHz */
             | RCC_CFGR_PPRE2_DIV1  /* APB2 = AHB / 1 = 72MHz */
             | RCC_CFGR_PPRE1_DIV2  /* APB1 = AHB / 2 = 36MHz */
             | RCC_CFGR_PLLSRC      /* PLL 源 = HSE */
             | RCC_CFGR_PLLMUL_9;   /* PLL 倍频 = 9 → 8MHz * 9 = 72MHz */

    /* 使能 PLL */
    RCC_CR |= RCC_CR_PLLON;
    { volatile int i = 0; while (!(RCC_CR & RCC_CR_PLLRDY) && i < 1000000) i++; }

    /* 切换系统时钟到 PLL */
    RCC_CFGR = (RCC_CFGR & ~0x3) | RCC_CFGR_SW_PLL;
    { volatile int i = 0;
      while ((RCC_CFGR & 0xC) != RCC_CFGR_SWS_PLL && i < 1000000) i++; }
}

/* ==================== UART（USART1 @ 115200 8N1） ==================== */

void stm32f103_uart_early_init(void)
{
    /* 使能 USART1 (bit14) 和 GPIOA (bit2) 时钟 */
    RCC_APB2ENR |= (1 << 14) | (1 << 2);

    /* 配置 PA9 (TX) = AF Push-Pull, 50MHz; PA10 (RX) = Input Floating */
    GPIOA_CRH = (GPIOA_CRH & 0xFFFFFF00) | 0x000000B4;

    /* USART1: 115200 8N1（72MHz / 16 / 115200 = 39.0625 → BRR = 39.0625 * 16 = 625） */
    USART1_BRR = 0x271;     /* 0x271 = 625 = 72MHz / 16 / 115200 */

    /* 使能 USART1, TX, RX */
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void arch_uart_init(uint64_t base, uint32_t baud)
{
    /* STM32F103 USART1 固定为 0x40013800 */
    (void)base;
    (void)baud;
    stm32f103_uart_early_init();
}

void arch_uart_putc(uint64_t base, char c)
{
    (void)base;
    while (!(USART1_SR & USART_SR_TXE));
    USART1_DR = (uint8_t)c;
    if (c == '\n') {
        while (!(USART1_SR & USART_SR_TXE));
        USART1_DR = '\r';
    }
}

char arch_uart_getc(uint64_t base)
{
    (void)base;
    while (!(USART1_SR & USART_SR_RXNE));
    return (char)(USART1_DR & 0xFF);
}

bool arch_uart_rx_ready(uint64_t base)
{
    (void)base;
    return (USART1_SR & USART_SR_RXNE) != 0;
}

bool arch_uart_tx_ready(uint64_t base)
{
    (void)base;
    return (USART1_SR & USART_SR_TXE) != 0;
}

uint64_t arch_uart_get_default_base(void)
{
    return 0x40013800;  /* USART1 */
}

/* ==================== CPU 控制 ==================== */

void arch_halt(void)
{
    __asm__ volatile("wfi");
}

void stm32f103_halt(void)
{
    __asm__ volatile("wfi");
}

void stm32f103_idle(void)
{
    __asm__ volatile("wfi");
}

/* ==================== 时间戳 (SysTick) ==================== */

#define STK_BASE            0xE000E010UL
#define STK_CTRL            (*(volatile uint32_t *)(STK_BASE + 0x00))
#define STK_LOAD            (*(volatile uint32_t *)(STK_BASE + 0x04))
#define STK_VAL             (*(volatile uint32_t *)(STK_BASE + 0x08))

#define STK_CTRL_ENABLE     (1 << 0)
#define STK_CTRL_TICKINT    (1 << 1)
#define STK_CTRL_CLKSOURCE  (1 << 2)
#define STK_CTRL_COUNTFLAG  (1 << 16)

static uint32_t g_tick_count = 0;

uint64_t stm32f103_get_timestamp(void)
{
    /* SysTick 计数 = 72MHz / 8 = 9MHz (如果使用 HCLK/8) */
    /* 返回 SysTick 当前值（递减计数） */
    return STK_VAL;
}

void stm32f103_udelay(uint32_t us)
{
    /* 简单忙等待: 72MHz → ~72 周期/微秒 */
    uint32_t ticks_per_us = 72;
    uint32_t total = us * ticks_per_us;
    while (total--) {
        __asm__ volatile("nop");
    }
}

/* SysTick 中断处理（由 C 代码调用） */
void stm32f103_systick_handler_c(void)
{
    g_tick_count++;
    extern void scheduler_tick(void);
    scheduler_tick();
}

/* SysTick EOI（清中断标志） */
void stm32_irq_eoi(void)
{
    /* 读 SysTick 的 CTRL 寄存器清 COUNTFLAG */
    volatile uint32_t ctrl = STK_CTRL;
    (void)ctrl;
}

/* ==================== 内存屏障 ==================== */

void stm32f103_memory_barrier(void)
{
    __asm__ volatile("dmb" ::: "memory");
}

void stm32f103_read_barrier(void)
{
    __asm__ volatile("dmb" ::: "memory");
}

void stm32f103_write_barrier(void)
{
    __asm__ volatile("dsb" ::: "memory");
}

/* ==================== 中断控制 ==================== */

bool stm32f103_disable_interrupts(void)
{
    uint32_t primask;
    __asm__ volatile("mrs %0, PRIMASK" : "=r"(primask));
    __asm__ volatile("cpsid i");
    return (primask & 1) == 0;  /* 之前未屏蔽返回 true */
}

void stm32f103_enable_interrupts(void)
{
    __asm__ volatile("cpsie i");
}

void stm32f103_restore_interrupts(bool state)
{
    if (state) {
        __asm__ volatile("cpsie i");
    }
}

/* ==================== 特权级查询 ==================== */

uint32_t stm32f103_get_privilege_level(void)
{
    uint32_t control;
    __asm__ volatile("mrs %0, CONTROL" : "=r"(control));
    return (control & 1) ^ 1;  /* 0=特权(Handler), 1=非特权(Thread) */
}

/* ==================== 调试 ==================== */

void stm32f103_breakpoint(void)
{
    __asm__ volatile("bkpt #0");
}

/* ==================== 系统调用 ==================== */

void arch_syscall_invoke(uint64_t num, uint64_t a1, uint64_t a2,
                         uint64_t a3, uint64_t a4)
{
    stm32f103_syscall_invoke(num, a1, a2, a3, a4);
}

void arch_syscall_return(uint64_t ret)
{
    __asm__ volatile("mov r0, %0; bx lr" : : "r"(ret) : "r0");
}

void arch_trigger_exception(uint32_t exc)
{
    (void)exc;
    __asm__ volatile("udf #0");
}

/* ==================== CPU ID ==================== */

cpu_id_t arch_get_cpu_id(void)
{
    return 0;  /* 单核，始终返回 0 */
}

/* ==================== 架构操作表 ==================== */

extern void context_switch(void *prev, void *next);
extern void stm32f103_context_init(void *ctx, void *entry, void *stack);

static const hal_arch_ops_t stm32f103_hal_ops = {
    .arch_name           = "STM32F103 (Cortex-M3)",
    .supports_io_ports   = false,

    /* 内存屏障 */
    .memory_barrier      = stm32f103_memory_barrier,
    .read_barrier        = stm32f103_read_barrier,
    .write_barrier       = stm32f103_write_barrier,

    /* 中断控制 */
    .disable_interrupts  = stm32f103_disable_interrupts,
    .enable_interrupts   = stm32f103_enable_interrupts,
    .restore_interrupts  = stm32f103_restore_interrupts,

    /* 时间 */
    .get_timestamp       = stm32f103_get_timestamp,
    .udelay              = stm32f103_udelay,

    /* 特权级 */
    .get_privilege_level = stm32f103_get_privilege_level,

    /* 上下文 */
    .save_context        = stm32f103_save_context,
    .restore_context     = stm32f103_restore_context,
    .context_switch      = context_switch,
    .context_init        = stm32f103_context_init,
    .context_init_flags  = NULL,  /* 使用默认 0 */

    /* 系统调用 */
    .syscall_invoke      = arch_syscall_invoke,
    .syscall_return      = arch_syscall_return,

    /* 异常 */
    .trigger_exception   = arch_trigger_exception,
    .halt                = stm32f103_halt,
    .idle                = stm32f103_idle,
    .breakpoint          = stm32f103_breakpoint,
    .stack_trace         = NULL,

    /* IO 端口 (Cortex-M3 无 IO 端口) */
    .inb                 = NULL,
    .inw                 = NULL,
    .inl                 = NULL,
    .outb                = NULL,
    .outw                = NULL,
    .outl                = NULL,

    /* CPU */
    .get_cpu_id          = arch_get_cpu_id,
};

/* ==================== 异常/中断分发 ==================== */

void panic_default_exception(uint32_t exc_return, uint32_t msp, uint32_t psp)
{
    console_puts("[PANIC] Default exception!\n");
    console_puts("  EXC_RETURN=0x");
    console_puthex32(exc_return);
    console_puts(" MSP=0x");
    console_puthex32(msp);
    console_puts(" PSP=0x");
    console_puthex32(psp);
    console_puts("\n");
}

void panic_hardfault(uint32_t exc_return, uint32_t sp)
{
    console_puts("[PANIC] HardFault!\n");
    console_puts("  EXC_RETURN=0x");
    console_puthex32(exc_return);
    console_puts(" SP=0x");
    console_puthex32(sp);

    /* 读取 CFSR (可配置故障状态寄存器) 诊断 */
    uint32_t cfsr = *(volatile uint32_t *)0xE000ED28;
    console_puts(" CFSR=0x");
    console_puthex32(cfsr);
    console_puts("\n");

    if (cfsr & (1 << 0))  console_puts("  -> IACCVIOL (指令访问冲突)\n");
    if (cfsr & (1 << 1))  console_puts("  -> DACCVIOL (数据访问冲突)\n");
    if (cfsr & (1 << 8))  console_puts("  -> MSTKERR (入栈错误)\n");
    if (cfsr & (1 << 9))  console_puts("  -> MUNSTKERR (出栈错误)\n");
    if (cfsr & (1 << 15)) console_puts("  -> BFARVALID (附总线地址)\n");

    uint32_t bfar = *(volatile uint32_t *)0xE000ED38;
    console_puts("  BFAR=0x");
    console_puthex32(bfar);
    console_puts("\n");
}

/* ==================== 中断分发 ==================== */

void stm32f103_irq_dispatch(uint32_t irq_num)
{
    extern volatile irq_route_entry_t irq_table[256];
    if (irq_num < 256 && irq_table[irq_num].handler_address) {
        typedef void (*handler_t)(void);
        ((handler_t)irq_table[irq_num].handler_address)();
    }
}

/* ==================== 架构初始化 ==================== */

void arch_hal_init(void)
{
    hal_register_arch_ops(&stm32f103_hal_ops);
    console_puts("[HAL] STM32F103 (Cortex-M3) architecture initialized\n");
}
