/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * STM32F103C8T6 链接脚本
 *
 * 内存布局：
 *   FLASH: 0x08000000 - 0x0800FFFF (64KB)
 *   SRAM:  0x20000000 - 0x20004FFF (20KB)
 *
 *   .isr_vector 在 Flash 起始位置（必须）
 *   .text 紧随其后
 *   .data 从 Flash 复制到 SRAM
 *   .bss  在 SRAM 中零初始化
 *   _estack 位于 SRAM 末尾
 *
 * 注：domain.c 等共享代码引用了以下符号，
 *     必须定义以确保链接通过：
 *   _text_start / _text_end
 *   _rodata_start / _rodata_end
 *   _data_start / _data_end
 *   __bss_start / __bss_end
 *   __stack_start / __stack_end
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* 栈顶指针 = SRAM 末尾 */
_estack = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    /* ==== 中断向量表（必须位于 Flash 起始地址） ==== */
    .isr_vector :
    {
        . = ALIGN(4);
        _text_start = .;
        KEEP(*(.vectors))
        . = ALIGN(4);
    } > FLASH

    /* ==== 代码段 ==== */
    .text :
    {
        . = ALIGN(4);

        *(.text.reset_handler)
        *(.text.default_exception_handler)
        *(.text.hardfault_handler)
        *(.text.pendsv_handler)
        *(.text.svcall_handler)
        *(.text.systick_handler)
        *(.text.default_irq_handler)
        *(.text)
        *(.text.*)

        . = ALIGN(4);
        _text_end = .;
    } > FLASH

    /* ==== 只读数据 ==== */
    .rodata :
    {
        . = ALIGN(4);
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
        _rodata_end = .;
    } > FLASH

    /* ==== .data 初始化数据（Flash 中存放初始值） ==== */
    _sidata = .;
    .data : AT(_sidata)
    {
        . = ALIGN(4);
        _data_start = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _data_end = .;
    } > SRAM

    /* ==== .bss 零初始化数据 ==== */
    .bss :
    {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > SRAM

    /* ==== 堆（可选） ==== */
    .heap (NOLOAD) :
    {
        . = ALIGN(4);
        _sheap = .;
        . = . + 1K;  /* 1KB 堆 */
        _eheap = .;
    } > SRAM

    /* ==== 栈（在 SRAM 末尾） ==== */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __stack_start = .;
        . = . + 2K;  /* 2KB 内核栈 */
        . = ALIGN(8);
        __stack_end = .;
    } > SRAM

    /* 丢弃 ARM 标准库的 .ARM 段 */
    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.attributes*)
        *(.comment)
        *(.debug*)
    }
}
