/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/* test_a: 仅输出单个字符。零外部符号依赖。 */

void __attribute__((used)) service_start(void) {
    __asm__ volatile("mov $0x41, %%al; mov $0x3f8, %%dx; outb %%al, %%dx" : : : "ax", "dx");
    while (1) __asm__ volatile("pause");
}
