/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

# HIC内核入口点（x86_64）
# Bootloader 将 TLV 写入 0x50000000 后跳转至此。

# PVH ELF note — 允许 QEMU -kernel 直接启动
.pushsection .note, "a", @note
.balign 4
.long 2f - 1f
.long 4f - 3f
.long 18                  # XEN_ELFNOTE_PHYS32_ENTRY
1: .asciz "Xen"
2: .balign 4
3: .long _start
4: .balign 4
.popsection

.section .text.start
.global _start
.global kernel_entry

.extern kernel_start
.extern __stack_end

# 串口寄存器常量
.set UART_LSR, 0x3FD
.set UART_THR, 0x3F8
.set UART_THRE, 0x20

# PVH/BIOS 32-bit entry: transition to long mode then jump to start64
.code32
_start:
kernel_entry:
    # 1. 零初始化页表区 (0x7000-0xAFFF, 16KB)
    mov $0x7000, %edi
    xor %eax, %eax
    mov $4096, %ecx
    cld
    rep stosl

    # 2. 建立临时页表 (PAE, 64-bit entries)
    mov $0x7000, %edi
    mov %edi, %cr3
    movl $0x8003, 0x7000; movl $0x0000, 0x7004   # PML4[0] -> PDPT
    movl $0x9003, 0x8000; movl $0x0000, 0x8004   # PDPT[0] -> PD
    movl $0xA003, 0x8008; movl $0x0000, 0x800C   # PDPT[1] -> PD2 (TLV)

    # Identity map 20MB (10 × 2MB huge pages) for kernel + data + stack
    movl $0x000083, 0x9000; movl $0x200083, 0x9008
    movl $0x400083, 0x9010; movl $0x600083, 0x9018
    movl $0x800083, 0x9020; movl $0xa00083, 0x9028
    movl $0xc00083, 0x9030; movl $0xe00083, 0x9038
    movl $0x1000083, 0x9040; movl $0x1200083, 0x9048
    movl $0x1400083, 0x9050; movl $0x1600083, 0x9058
    movl $0x1800083, 0x9060; movl $0x1a00083, 0x9068
    movl $0x1c00083, 0x9070; movl $0x1e00083, 0x9078

    # Map TLV at 0x50000000
    movl $0x50000083, 0xA400  # PD2[128]=0x50000000 2MB page

    # 3. Enable PAE, LME, paging
    mov %cr4, %eax; or $0x20, %eax; mov %eax, %cr4
    mov $0xC0000080, %ecx; rdmsr; or $0x100, %eax; wrmsr
    mov %cr0, %eax; or $0x80000001, %eax; mov %eax, %cr0

    # 4. Load GDT (含64位段) & far jump to 64-bit mode
    lgdt gdt_ptr_32
    ljmp $0x08, $start64

.code64
start64:
    mov $0x10, %ax; mov %ax, %ss; mov %ax, %ds; mov %ax, %es
    lgdt gdt_ptr(%rip)
    lea __stack_end(%rip), %rsp
    and $~0xF, %rsp
    call kernel_start

    cli
.kernel_panic_loop:
    hlt
    jmp .kernel_panic_loop

# 32-bit 串口输出
.code32
.serial_putchar_32:
    push %edx
    push %eax
    mov $UART_LSR, %dx
.serial_putchar_wait_32:
    in %dx, %al
    test $UART_THRE, %al
    jz .serial_putchar_wait_32
    mov $UART_THR, %dx
    pop %eax
    out %al, %dx
    pop %edx
    ret

# 64-bit 串口输出
.code64
.serial_putchar_64:
    push %rdx
    push %rax
    mov $UART_LSR, %dx
.serial_putchar_wait_64:
    in %dx, %al
    test $UART_THRE, %al
    jz .serial_putchar_wait_64
    mov $UART_THR, %dx
    pop %rax
    out %al, %dx
    pop %rdx
    ret

/* 32-bit 过渡用 GDT（含64位段，在.text中以便32位代码访问） */
.balign 8
gdt_ptr_32:
    .word gdt_64_end - gdt_64 - 1
    .long gdt_64
gdt_64:
    .quad 0x0000000000000000                    /* 0: NULL描述符 */
    .quad 0x00af9a000000ffff                    /* 8: 内核代码段（64位） */
    .quad 0x00cf92000000ffff                    /* 16: 内核数据段 */
gdt_64_end:

/* 数据段 */
.section .data

/* GDT指针（完整实现） */
gdt_ptr:
    .word gdt_end - gdt - 1
    .quad gdt

/* GDT（完整实现） */
gdt:
    .quad 0x0000000000000000                    /* 0: NULL描述符 */
    .quad 0x00af9a000000ffff                    /* 1: 内核代码段（64位） */
    .quad 0x00cf92000000ffff                    /* 2: 内核数据段 */
    .quad 0x00cffa000000ffff                    /* 3: 用户代码段（64位） */
    .quad 0x00cff2000000ffff                    /* 4: 用户数据段 */
    .quad 0x0000000000000000                    /* 5: TSS描述符（占位） */
gdt_end:
