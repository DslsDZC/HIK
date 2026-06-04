/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/* test_b: 读共享环形缓冲区。零外部符号依赖。 */

static void put(const char *s) {
    unsigned short p = 0x3F8;
    for (; *s; s++) __asm__ volatile("outb %0, %w1" : : "a"((unsigned char)*s), "Nd"(p));
}

static void puthex(unsigned long v) {
    unsigned short p = 0x3F8;
    const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 16; i++)
        __asm__ volatile("outb %0, %w1" : : "a"((unsigned char)hex[(v >> (60 - i*4)) & 0xF]), "Nd"(p));
}

struct abi { unsigned long code_base, partner_entry, shmem_addr; };

void __attribute__((used)) service_start(void) {
    struct abi *a;
    __asm__("lea -0x27(%%rip), %0" : "=r"(a));

    put("[B] started shmem="); puthex(a->shmem_addr); put("\n");

    if (a->partner_entry) {
        put("[B] calling partner IPC...\n");
        void (*fn)(void) = (void (*)(void))(unsigned long)a->partner_entry;
        fn();
        put("[B] IPC returned\n");
    }

    if (a->shmem_addr) {
        volatile unsigned int *idx = (volatile unsigned int *)(unsigned long)a->shmem_addr;
        unsigned char *data = (unsigned char *)(unsigned long)a->shmem_addr + 8;
        unsigned int ri = idx[1], wi = idx[0];
        put("[B] ring ri="); puthex(ri); put(" wi="); puthex(wi); put("\n");
        if (wi > ri) {
            put("[B] data: ");
            for (unsigned int i = ri; i < wi; i++)
                __asm__ volatile("outb %0, %w1" : : "a"(data[i & 0xFFF]), "Nd"((unsigned short)0x3F8));
            put("\n");
        }
    }

    while (1) __asm__ volatile("pause; pause; pause");
}
