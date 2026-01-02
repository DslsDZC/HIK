/* HIK Core-1 Service Linker Script */

ENTRY(_service_entry)

SECTIONS
{
    /* Code section */
    . = 0x100000;  /* Starting address (1MB) */
    .text : {
        *(.text)
        *(.text.*)
    }

    /* Read-only data */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /* Read-write data (initialized) */
    .data : {
        *(.data)
        *(.data.*)
    }

    /* Read-write data (uninitialized) */
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /* Stack */
    .stack : {
        . = ALIGN(16);
        *(.stack)
        . = . + 0x10000;  /* 64KB stack */
    }

    /* Discard sections */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}