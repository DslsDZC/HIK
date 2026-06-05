/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/* test_b: 串口输出。零外部符号依赖。 */

static void put(const char *s) {
    unsigned short p = 0x3F8;
    for (; *s; s++) __asm__ volatile("outb %0, %w1" : : "a"((unsigned char)*s), "Nd"(p));
}

void __attribute__((used)) service_start(void) {
    put("[B] started\n");

    while (1) __asm__ volatile("pause; pause; pause");
}
