/* HIK UEFI Bootloader Linker Script */

ENTRY(_start)

SECTIONS
{
    . = 0;
    
    .text : {
        *(.text)
        *(.text.*)
    }
    
    . = ALIGN(4096);
    
    .sdata : {
        *(.sdata)
        *(.sdata.*)
    }
    
    . = ALIGN(4096);
    
    .data : {
        *(.data)
        *(.data.*)
    }
    
    . = ALIGN(4096);
    
    .dynamic : {
        *(.dynamic)
    }
    
    . = ALIGN(4096);
    
    .rela : {
        *(.rela*)
    }
    
    . = ALIGN(4096);
    
    .reloc : {
        *(.reloc)
    }
    
    . = ALIGN(4096);
    
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    
    . = ALIGN(4096);
    
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
    }
}