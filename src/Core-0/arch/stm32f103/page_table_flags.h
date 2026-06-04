/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * STM32F103 (Cortex-M3) 页表标志
 *
 * Cortex-M3 无 MMU/页表。此文件仅用于编译通过，
 * 实际使用 MPU 区域控制内存访问。
 */

#ifndef HIC_ARCH_PAGE_TABLE_FLAGS_H
#define HIC_ARCH_PAGE_TABLE_FLAGS_H

/* 无实际页表标志 — 所有 pagetable_* 函数为桩 */
#define PAGE_TABLE_FLAG_PRESENT   0
#define PAGE_TABLE_FLAG_WRITABLE  0
#define PAGE_TABLE_FLAG_USER      0
#define PAGE_TABLE_FLAG_NX        0

/* 页表级别数 */
#define PAGE_TABLE_LEVELS         0

#endif /* HIC_ARCH_PAGE_TABLE_FLAGS_H */
