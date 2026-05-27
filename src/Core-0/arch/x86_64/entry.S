/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

# HIC内核入口点（x86_64）
# Bootloader 将 TLV 写入 0x1000 后跳转至此。

.section .text.start
.global _start
.global kernel_entry

.extern kernel_start
.extern __stack_end

# 串口寄存器常量
.set UART_LSR, 0x3FD
.set UART_THR, 0x3F8
.set UART_THRE, 0x20

_start:
kernel_entry:
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15

    # 串口输出 'K' 表示内核入口
    mov $'K', %al
    call .serial_putchar

    # 输出 '1'、'2' 调试标记
    mov $'1', %al
    call .serial_putchar

    # 加载 GDT
    mov $'2', %al
    call .serial_putchar
    lgdt gdt_ptr

    # 使用内核内置栈（链接脚本 .stack 段）
    mov $'3', %al
    call .serial_putchar
    lea __stack_end(%rip), %rsp
    and $~0xF, %rsp

    # 跳转到 C 入口
    mov $'4', %al
    call .serial_putchar
    call kernel_start

    cli
.kernel_panic_loop:
    hlt
    jmp .kernel_panic_loop

# 串口输出字符函数（内联）
# 输入: %al = 字符
.serial_putchar:
    push %rdx
    push %rax
    mov $UART_LSR, %dx        # 串口线路状态寄存器
.serial_putchar_wait:
    in %dx, %al               # 读取状态
    test $UART_THRE, %al      # 检查发送保持寄存器是否为空
    jz .serial_putchar_wait
    mov $UART_THR, %dx        # 串口数据寄存器
    pop %rax                  # 恢复字符
    out %al, %dx              # 输出字符
    pop %rdx
    ret

/* 数据段 */
.section .data

/* GDT指针（完整实现） */
gdt_ptr:
    .word gdt_end - gdt - 1  /* GDT限制 */
    .quad gdt                /* GDT基址 */

/* GDT（完整实现） */
gdt:
    .quad 0x0000000000000000                    /* 0: NULL描述符 */
    .quad 0x00af9a000000ffff                    /* 1: 内核代码段（64位） */
    .quad 0x00cf92000000ffff                    /* 2: 内核数据段 */
    .quad 0x00cffa000000ffff                    /* 3: 用户代码段（64位） */
    .quad 0x00cff2000000ffff                    /* 4: 用户数据段 */
    .quad 0x0000000000000000                    /* 5: TSS描述符（占位） */
gdt_end: