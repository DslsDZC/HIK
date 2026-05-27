/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核主入口
 *
 * Bootloader 将 TLV 引导信息写入 BOOT_INFO_ADDR，
 * 内核读取并解析，不做任何硬件探测。
 * 无旧 struct、无回退、无兼容路径。
 */

#include "boot_info.h"
#include "types.h"
#include "hal.h"

extern void kernel_main(void);

/* 最小化引导信息（无 TLV 时使用，仅供测试） */
extern hic_boot_info_t *g_boot_info;
extern void boot_info_init_minimal(void);

void kernel_start(void) {
    hal_init();
    hal_uart_init(NULL);

    boot_info_parse_tlv();
    if (g_boot_info == NULL) {
        while (1) hal_halt();
    }

    kernel_main();

    while (1) {
        hal_halt();
    }
}
