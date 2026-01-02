/* HIK Core-3 Application Linker Script */

ENTRY(_app_entry)

SECTIONS
{
    /* Code section */
    . = 0x400000;  /* Starting address (4MB) */
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