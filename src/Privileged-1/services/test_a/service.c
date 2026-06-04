/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/* test_a: 写共享环形缓冲区。零外部符号依赖，全部内联。 */

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
    /* ABI 地址 = 本条指令地址 - 0x20。lea 0(%rip) 取本指令 +7 的地址。 */
    struct abi *a;
    __asm__("lea 0(%%rip), %0\n sub $0x27, %0" : "=r"(a));

    put("[A] base="); puthex(a->code_base); put("\n");

    if (a->shmem_addr) {
        volatile unsigned int *idx = (volatile unsigned int *)(unsigned long)a->shmem_addr;
        unsigned char *data = (unsigned char *)(unsigned long)a->shmem_addr + 8;
        const char *msg = "[A->B] via ringbuf!";
        unsigned int wi = idx[0];
        for (int i = 0; msg[i]; i++) data[(wi + i) & 0xFFF] = (unsigned char)msg[i];
        idx[0] = wi + 20;
        put("[A] wrote ringbuf\n");
    } else { put("[A] no shmem\n"); }

    while (1) __asm__ volatile("pause; pause; pause");
}
