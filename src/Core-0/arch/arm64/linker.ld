OUTPUT_FORMAT(elf64-littleaarch64)
ENTRY(_start)

/* QEMU virt 平台默认加载地址 */
KERNEL_BASE = 0x40080000;
STATIC_MODULES_BASE = 0x43000000;

SECTIONS
{
    . = KERNEL_BASE;

    .text ALIGN(4K) : AT(ADDR(.text)) {
        _text_start = .;
        *(.text.start)
        *(.text)
        *(.text.*)
        _text_end = .;
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata)) {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        _rodata_end = .;
    }

    .data ALIGN(4K) : AT(ADDR(.data)) {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    }

    .bss ALIGN(4K) : AT(ADDR(.bss)) {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4K);
        __bss_end = .;
    }

    .stack ALIGN(4K) : AT(ADDR(.stack)) {
        __stack_start = .;
        . += 16K;
        __stack_end = .;
    }

    .lbss ALIGN(4K) : AT(ADDR(.lbss)) {
        __lbss_start = .;
        *(.lbss)
        *(.lbss.*)
        . = ALIGN(4K);
        __lbss_end = .;
    }

    _kernel_end = .;
    _kernel_size = _kernel_end - KERNEL_BASE;

    . = STATIC_MODULES_BASE;
    _static_modules_region_start = .;

    .static_modules ALIGN(4K) : AT(ADDR(.static_modules)) {
        __static_modules_start = .;
        KEEP(*(.static_modules))
        __static_modules_end = .;
    }

    .static_services_text ALIGN(4K) : AT(ADDR(.static_services_text)) {
        _static_modules_text_start = .;
        *(.static_svc.*.text)
        *(.static_svc.*.rodata)
        _static_modules_text_end = .;
    }

    .static_services_data ALIGN(4K) : AT(ADDR(.static_services_data)) {
        _static_modules_data_start = .;
        *(.static_svc.*.data)
        _static_modules_data_end = .;
    }

    .static_services_bss ALIGN(4K) : AT(ADDR(.static_services_bss)) {
        _static_modules_bss_start = .;
        *(.static_svc.*.bss)
        _static_modules_bss_end = .;
    }

    _static_modules_region_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
