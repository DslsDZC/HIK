/*
 * HIK Core-0 Kernel Linker Script
 * 
 * This linker script defines the memory layout for the Core-0 kernel.
 */

ENTRY(kernel_init)

SECTIONS
{
    /* Kernel code section */
    .text 0x100000 : {
        *(.text)
        *(.text.*)
    }

    /* Read-only data section */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /* Read-write data section */
    .data : {
        *(.data)
        *(.data.*)
    }

    /* BSS section */
    .bss : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Stack section */
    .stack : {
        . = ALIGN(16);
        *(.stack)
    }

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