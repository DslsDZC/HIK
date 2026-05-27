/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC ARM64 架构特定 HAL 实现
 */

#include <stdint.h>
#include <stdbool.h>
#include "hal.h"
#include "irq.h"
#include "ipc3.h"
#include "lib/console.h"

/* ==================== 前向声明 ==================== */

uint64_t arm64_get_timestamp(void);
void arm64_halt(void);
void arm64_idle(void);
void arm64_save_context(void *ctx);
void arm64_restore_context(void *ctx);
extern void arm64_context_init(void *ctx, void *entry, void *stack);
extern void context_switch(void *prev, void *next);

/* ==================== 微秒延迟 ==================== */

void arm64_udelay(uint32_t us) {
    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    if (freq == 0) freq = 24000000;
    uint64_t target = arm64_get_timestamp() + (freq / 1000000) * us;
    while (arm64_get_timestamp() < target);
}

/* ==================== CPU 控制 & 时间戳 ==================== */

void arch_halt(void) { __asm__ volatile("wfi"); }
void arm64_halt(void) { __asm__ volatile("wfi"); }
void arm64_idle(void) { __asm__ volatile("yield"); }

uint64_t arm64_get_timestamp(void) {
    uint64_t cnt;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(cnt));
    return cnt;
}

/* ==================== 内存屏障 ==================== */

void arm64_memory_barrier(void) { __asm__ volatile("dmb ish" ::: "memory"); }
void arm64_read_barrier(void)   { __asm__ volatile("dmb ishld" ::: "memory"); }
void arm64_write_barrier(void)  { __asm__ volatile("dmb ish" ::: "memory"); }

/* ==================== 中断控制 ==================== */

bool arm64_disable_interrupts(void) {
    uint64_t daif;
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    __asm__ volatile("msr daifset, #2");
    return (daif & (1 << 7)) == 0;
}

void arm64_enable_interrupts(void)  { __asm__ volatile("msr daifclr, #2"); }

void arm64_restore_interrupts(bool state) {
    if (state) __asm__ volatile("msr daifclr, #2");
}

/* ==================== 特权级查询 ==================== */

uint32_t arm64_get_privilege_level(void) {
    uint64_t el;
    __asm__ volatile("mrs %0, currentel" : "=r"(el));
    return (el >> 2) & 3;
}

/* ==================== 调试 ==================== */

void arm64_breakpoint(void) { __asm__ volatile("brk #0"); }

/* ==================== 系统调用 ==================== */

void arch_syscall_invoke(u64 num, u64 a1, u64 a2, u64 a3, u64 a4) {
    __asm__ volatile(
        "mov x8, %0\n mov x0, %1\n mov x1, %2\n mov x2, %3\n mov x3, %4\n svc #0\n"
        : : "r"(num), "r"(a1), "r"(a2), "r"(a3), "r"(a4)
        : "x0", "x1", "x2", "x3", "x8", "memory");
}

void arch_syscall_return(u64 ret) {
    __asm__ volatile("mov x0, %0\n eret\n" : : "r"(ret) : "x0");
}

void arch_trigger_exception(u32 exc) {
    (void)exc;
    __asm__ volatile("brk #1");
}

/* ==================== CPU ID ==================== */

cpu_id_t arch_get_cpu_id(void) {
    u64 mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (cpu_id_t)(mpidr & 0xFF);
}

/* ==================== UART（PL011） ==================== */

#define PL011_DR    0x000
#define PL011_FR    0x018
#define PL011_FR_TXFF (1 << 5)

static volatile u32 *pl011_base = (u32 *)0x9000000;

void arch_uart_init(uint64_t base, uint32_t baud) {
    pl011_base = (u32 *)base;
    /* PL011 initialization: enable UART, TX, RX */
    volatile u32 *uart = (u32 *)base;
    u32 divider = 1843200 / (16 * baud);
    u32 frac = (1843200 % (16 * baud)) * 64 / (16 * baud);
    uart[0x024 / 4] = divider;               /* IBRD */
    uart[0x028 / 4] = frac;                  /* FBRD */
    uart[0x02C / 4] = 0x60;                  /* LCR_H: 8bit, FIFO enabled */
    uart[0x030 / 4] = 0x301;                 /* CR: UARTEN, TXE, RXE */
}

void arch_uart_putc(uint64_t base, char c) {
    volatile u32 *uart = (u32 *)base;
    while (uart[PL011_FR / 4] & PL011_FR_TXFF);
    uart[PL011_DR / 4] = c;
    if (c == '\n') {
        while (uart[PL011_FR / 4] & PL011_FR_TXFF);
        uart[PL011_DR / 4] = '\r';
    }
}

char arch_uart_getc(uint64_t base) {
    (void)base;
    return 0;
}

bool arch_uart_rx_ready(uint64_t base) { (void)base; return false; }
bool arch_uart_tx_ready(uint64_t base) { (void)base; return true; }

uint64_t arch_uart_get_default_base(void) { return 0x09000000; }

/* ==================== 架构操作表 ==================== */

extern void context_switch(void *prev, void *next);
static const hal_arch_ops_t arm64_hal_ops = {
    .arch_name          = "ARM64",
    .supports_io_ports  = false,
    .halt               = arm64_halt,
    .idle               = arm64_idle,
    .memory_barrier     = arm64_memory_barrier,
    .read_barrier       = arm64_read_barrier,
    .write_barrier      = arm64_write_barrier,
    .disable_interrupts = arm64_disable_interrupts,
    .enable_interrupts  = arm64_enable_interrupts,
    .restore_interrupts = arm64_restore_interrupts,
    .get_timestamp      = arm64_get_timestamp,
    .udelay             = arm64_udelay,
    .get_privilege_level = arm64_get_privilege_level,
    .save_context       = arm64_save_context,
    .restore_context    = arm64_restore_context,
    .context_switch     = context_switch,
    .context_init       = arm64_context_init,
    .syscall_invoke     = arch_syscall_invoke,
    .syscall_return     = arch_syscall_return,
    .trigger_exception  = arch_trigger_exception,
    .breakpoint         = arm64_breakpoint,
    .get_cpu_id         = arch_get_cpu_id,
};

/* ==================== 异常/中断处理（供 interrupts.S 调用） ==================== */

void arm64_exception_dispatch(uint64_t *regs, uint32_t ec) {
    switch (ec) {
    case 0x15: /* SVC from EL0 */
        /* IPC 3.0 替代了 SVC，不应到达此处 */
        break;
    case 0x20: /* Instruction Abort */
    case 0x21: /* Data Abort */
        console_puts("[EXC] Page fault at 0x");
        console_puthex64(regs[17]); /* ELR_EL1 */
        console_puts("\n");
        if (ipc3_handle_pf(regs[16] /* FAR_EL1 */, 0, regs))
            return;
        break;
    case 0x01: /* WF* trap */
        break;
    default:
        console_puts("[EXC] Unhandled EC=0x");
        console_puthex32(ec);
        console_puts(" at 0x");
        console_puthex64(regs[17]);
        console_puts("\n");
        break;
    }
    while (1) { __asm__ volatile("wfi"); }
}

void arm64_irq_dispatch(uint64_t *regs) {
    (void)regs;
    extern void gic_init(void);
    extern u32 gic_read_iar(void);
    extern void gic_write_eoi(u32);
    u32 irq = gic_read_iar();
    extern volatile irq_route_entry_t irq_table[256];
    if (irq < 256 && irq_table[irq].handler_address) {
        typedef void (*handler_t)(void);
        ((handler_t)irq_table[irq].handler_address)();
    }
    gic_write_eoi(irq);
}

void arm64_switch_pagetable(void *pagetable) {
    if (pagetable) {
        __asm__ volatile("msr ttbr0_el1, %0; isb" : : "r"(pagetable));
    }
}

/* ==================== 架构初始化 ==================== */

void arch_hal_init(void) {
    hal_register_arch_ops(&arm64_hal_ops);

    extern void gic_init(void);
    gic_init();

    console_puts("[HAL] ARM64 architecture initialized\n");
}
