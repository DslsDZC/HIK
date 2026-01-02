/*
 * HIK Core-0 Kernel Linker Script
 *
 * This linker script defines the memory layout for the Core-0 kernel.
 */

ENTRY(_start)

PHDRS
{
    text PT_LOAD FILEHDR PHDRS FLAGS(0x7); /* Read + Write + Execute */
}

SECTIONS
{
    /* Start at 1MB */
    . = 0x100000;

    /* Multiboot header - MUST be first, in first 8KB and aligned on 4-byte boundary */
    .multiboot ALIGN(4) : {
        *(.multiboot)
    } :text

    /* Kernel code section */
    .text : {
        *(.text)
        *(.text.*)
    } :text

    /* Read-only data section */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } :text

    /* Read-write data section */
    .data : {
        *(.data)
        *(.data.*)
    } :text

    /* BSS section */
    .bss : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :text

    /* Stack section */
    .stack : {
        . = ALIGN(16);
        *(.stack)
    } :text

    /* Page tables */
    . = ALIGN(4096);
    .pml4 : {
        *(.pml4)
    }

    /* Discard sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}