/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/* test_a: just print 'A' to test if thread runs at all */

void __attribute__((used)) service_start(void) {
    __asm__ volatile("mov $0x41, %%al; mov $0x3f8, %%dx; outb %%al, %%dx" : : : "ax", "dx");
    while (1) __asm__ volatile("pause");
}
