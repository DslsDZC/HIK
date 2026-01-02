/*
 * HIK BIOS Bootloader - Stage 2 Startup
 * 
 * This is the entry point for stage 2 bootloader.
 * It sets up the stack and calls the C main function.
 */

.section .text
.global _start
.code32

_start:
    /* Setup stack */
    movl $0x90000, %esp
    
    /* Clear BSS */
    movl $_bss_start, %edi
    movl $_bss_end, %ecx
    subl %edi, %ecx
    xorl %eax, %eax
    rep stosb
    
    /* Call C main function */
    call stage2_main
    
    /* Should never reach here */
    hlt
    jmp _start

/*
 * Switch to long mode and jump to kernel
 * %rdi = kernel entry point
 * %rsi = boot info structure address
 */
.global switch_to_long_mode
switch_to_long_mode:
    /* Save boot info pointer */
    push %rsi
    
    /* Setup long mode page tables */
    call setup_page_tables
    
    /* Enable PAE */
    movl %cr4, %eax
    orl $0x20, %eax           /* Set PAE bit */
    movl %eax, %cr4
    
    /* Load PML4 */
    movl $pml4_table, %eax
    movl %eax, %cr3
    
    /* Enable long mode */
    movl $0xC0000080, %ecx    /* MSR_EFER */
    rdmsr
    orl $0x1000, %eax         /* Set LME bit */
    wrmsr
    
    /* Enable paging */
    movl %cr0, %eax
    orl $0x80000000, %eax     /* Set PG bit */
    movl %eax, %cr0
    
    /* Load 64-bit GDT */
    lgdt gdtr64
    
    /* Far jump to 64-bit code */
    ljmp $0x08, $long_mode_start

.code64
long_mode_start:
    /* Setup 64-bit segments */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    
    /* Restore boot info pointer to %rsi */
    pop %rsi
    
    /* Setup stack */
    movl $0x90000, %esp
    
    /* Jump to kernel entry point */
    jmp *%rdi
    
    /* Should never reach here */
    hlt
    jmp long_mode_start

/*
 * Setup page tables for long mode
 */
setup_page_tables:
    /* Clear PML4 table */
    movl $pml4_table, %edi
    movl $4096 / 4, %ecx
    xorl %eax, %eax
    rep stosl
    
    /* Clear PDP table */
    movl $pdp_table, %edi
    movl $4096 / 4, %ecx
    xorl %eax, %eax
    rep stosl
    
    /* Clear PD table */
    movl $pd_table, %edi
    movl $4096 / 4, %ecx
    xorl %eax, %eax
    rep stosl
    
    /* Setup PML4 entry pointing to PDP */
    movl $pdp_table, %eax
    orl $0x3, %eax             /* Present + Writable */
    movl %eax, pml4_table
    
    /* Setup PDP entry pointing to PD */
    movl $pd_table, %eax
    orl $0x3, %eax             /* Present + Writable */
    movl %eax, pdp_table
    
    /* Setup PD entries with 2MB pages */
    movl $pd_table, %edi
    movl $512, %ecx            /* 512 entries */
    movl $0x83, %eax           /* Present + Writable + 2MB page */
    
setup_pd_loop:
    stosl
    addl $0x200000, %eax       /* Next 2MB */
    loop setup_pd_loop
    
    ret

/*
 * Data section
 */
.section .data

/* 64-bit GDT */
.align 16
gdt64:
    /* Null descriptor */
    .quad 0
    
    /* Code descriptor (64-bit, ring 0) */
    .word 0xFFFF
    .word 0
    .byte 0
    .byte 0x9A                 /* Present, DPL=0, Code, Execute/Read */
    .byte 0xAF                 /* 64-bit, 4KB granularity */
    .byte 0
    
    /* Data descriptor (64-bit, ring 0) */
    .word 0xFFFF
    .word 0
    .byte 0
    .byte 0x92                 /* Present, DPL=0, Data, Read/Write */
    .byte 0xCF                 /* 32-bit, 4KB granularity */
    .byte 0

gdt64_end:

/* GDTR for 64-bit GDT */
gdtr64:
    .word gdt64_end - gdt64 - 1
    .quad gdt64

/* Page tables (aligned to 4KB) */
.align 4096
pml4_table:
    .skip 4096

.align 4096
pdp_table:
    .skip 4096

.align 4096
pd_table:
    .skip 4096

/* BSS section */
.section .bss
.align 4
_bss_start:
_bss_end: