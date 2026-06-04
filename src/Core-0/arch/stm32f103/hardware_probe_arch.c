/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * STM32F103 硬件探测
 *
 * 探测 MCU 的硬件特性：
 *   - CPUID 读取（识别 Cortex-M3 r1p1/r2p0）
 *   - Flash 和 SRAM 大小
 *   - 外设存在检测
 */

#include "hardware_probe.h"
#include "lib/console.h"
#include "lib/mem.h"

/* ==================== 寄存器定义 ==================== */

#define SCB_CPUID           (*(volatile uint32_t *)0xE000ED00)

/* SCB_CPUID 位 */
#define CPUID_IMPLEMENTER_MASK  (0xFF << 24)
#define CPUID_VARIANT_MASK      (0xF << 20)
#define CPUID_ARCHITECTURE_MASK (0xF << 16)
#define CPUID_PARTNO_MASK       (0xFFF << 4)
#define CPUID_REVISION_MASK     (0xF)

#define CPUID_IMPLEMENTER_ARM   (0x41 << 24)
#define CPUID_ARCH_ARMv7M       (0xC << 20)  /* 实际上 variant 字段 */
#define CPUID_PARTNO_CORTEX_M3  (0xC23 << 4)

/* ==================== Flash 大小寄存器 ==================== */

#define FLASH_SIZE_REG      (*(volatile uint16_t *)0x1FFFF7E0)

/* ==================== Id 寄存器 ==================== */

#define MCU_IDCODE          (*(volatile uint32_t *)0xE0042000)

/* ==================== 硬件探测 ==================== */

void hardware_probe(void)
{
    console_puts("[HW] Probing STM32F103...\n");

    /* 读取 CPUID */
    uint32_t cpuid = SCB_CPUID;
    uint32_t implementer = (cpuid & CPUID_IMPLEMENTER_MASK) >> 24;
    uint32_t partno = (cpuid & CPUID_PARTNO_MASK) >> 4;
    uint32_t revision = cpuid & CPUID_REVISION_MASK;

    console_puts("  CPUID: 0x");
    console_puthex32(cpuid);
    console_puts("\n");

    if (implementer == 0x41) {
        console_puts("  Implementer: ARM\n");
    }

    console_puts("  Part No: 0x");
    console_puthex32(partno);
    console_puts("\n");

    if (partno == 0xC23) {
        console_puts("  Core: Cortex-M3\n");
    }

    console_puts("  Revision: r");
    console_putu32((cpuid & CPUID_VARIANT_MASK) >> 20);
    console_puts("p");
    console_putu32(revision);
    console_puts("\n");

    /* 读取 Flash 大小 */
    uint16_t flash_kb = FLASH_SIZE_REG;
    console_puts("  Flash: ");
    console_putu32(flash_kb);
    console_puts(" KB\n");

    /* 读取 MCU IDCODE */
    uint32_t idcode = MCU_IDCODE;
    console_puts("  MCU IDCODE: 0x");
    console_puthex32(idcode);
    console_puts("\n");

    console_puts("  SRAM: 20 KB\n");
    console_puts("  Max Freq: 72 MHz\n");
    console_puts("[HW] STM32F103 probe complete\n");
}

/* ==================== 架构特定探测（空/调试用） ==================== */

uint32_t hardware_probe_get_feature_flags(void)
{
    /* STM32F103 特性标志 */
    return 0;  /* 无 MMU，无 SMP，无 xAPIC */
}

bool hardware_probe_has_feature(uint32_t feature)
{
    (void)feature;
    return false;
}

const char *hardware_probe_get_cpu_name(void)
{
    return "Cortex-M3 @ STM32F103C8T6";
}

uint32_t hardware_probe_get_cpu_freq(void)
{
    return 72000000;  /* 72 MHz */
}

uint32_t hardware_probe_get_cache_line_size(void)
{
    return 4;  /* Cortex-M3 无数据缓存 */
}
