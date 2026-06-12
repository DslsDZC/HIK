/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/* test_a: 测试模块，通过 IPC3 调用 test_b。导入的符号由加载器重定位解析。 */

extern unsigned long long module_get_service_entry(const char *name);
extern void module_thread_yield(void);

static void put(const char *s) {
    unsigned short p = 0x3F8;
    for (; *s; s++) __asm__ volatile("outb %0, %w1" : : "a"((unsigned char)*s), "Nd"(p));
}

void __attribute__((used)) service_start(void) {
    put("[A] started\n");

    /* 运行时查找 test_b 的 IPC 入口页地址 */
    unsigned long long entry_va = module_get_service_entry("TEST_B");

    if (entry_va) {
        put("[A] calling test_b via IPC...\n");
        ((void (*)(void))(unsigned long)entry_va)();
        put("[A] IPC returned!\n");
    } else {
        put("[A] test_b not found\n");
    }

    while (1) {
        for (volatile int i = 0; i < 100000; i++) __asm__ volatile("pause");
        module_thread_yield();
    }
}
