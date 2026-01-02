/*
 * HIK Core-0 Long Mode Support (Assembly)
 * 
 * This file contains assembly code for long mode initialization.
 */

/* Page table addresses */
.equ PML4_BASE,  0x10000
.equ PDP_BASE,   0x11000
.equ PD_BASE,    0x12000
.equ PT_BASE,    0x13000

.section .text
.code32

/* Check if CPU supports long mode */
.global longmode_check_support
.type longmode_check_support, @function
longmode_check_support:
    /* Check CPUID support */
    pushfl
    popl    %eax
    movl    %eax, %ecx
    xorl    $0x200000, %eax
    pushl   %eax
    popfl
    pushfl
    popl    %eax
    pushl   %ecx
    popfl
    xorl    %ecx, %eax
    jz      no_cpuid
    
    /* Get CPUID max leaf */
    movl    $0, %eax
    cpuid
    cmpl    $0x80000001, %eax
    jb      no_longmode
    
    /* Check for long mode support */
    movl    $0x80000001, %eax
    cpuid
    testl   $0x20000000, %edx
    jz      no_longmode
    
    /* Long mode supported */
    movl    $1, %eax
    ret

no_longmode:
    xorl    %eax, %eax
    ret

no_cpuid:
    xorl    %eax, %eax
    ret

/* Enable PAE */
.global longmode_enable_pae
.type longmode_enable_pae, @function
longmode_enable_pae:
    /* Enable PAE in CR4 */
    movl    %cr4, %eax
    orl     $0x20, %eax
    movl    %eax, %cr4
    ret

/* Setup page tables */
.global longmode_setup_page_tables
.type longmode_setup_page_tables, @function
longmode_setup_page_tables:
    /* Clear page tables */
    movl    $PML4_BASE, %edi
    movl    $0x3000 / 4, %ecx
    xorl    %eax, %eax
    rep stosl
    
    /* Setup PML4 - identity map first 512GB */
    movl    $PML4_BASE, %edi
    movl    $PDP_BASE, %eax
    orl     $0x3, %eax    /* Present + Writable */
    movl    %eax, (%edi)
    
    /* Setup PDP - map first 1GB */
    movl    $PDP_BASE, %edi
    movl    $PD_BASE, %eax
    orl     $0x3, %eax
    movl    %eax, (%edi)
    
    /* Setup PD - map first 2MB with 4KB pages */
    movl    $PD_BASE, %edi
    movl    $PT_BASE, %eax
    orl     $0x3, %eax
    movl    %eax, (%edi)
    
    /* Setup PT - identity map first 4KB */
    movl    $PT_BASE, %edi
    movl    $0x3, %eax    /* Present + Writable */
    movl    %eax, (%edi)
    
    ret

/* Enable long mode */
.global longmode_enable
.type longmode_enable, @function
longmode_enable:
    /* Set EFER.LME */
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $0x1000, %eax
    wrmsr
    
    /* Enable paging in CR0 */
    movl    %cr0, %eax
    orl     $0x80000001, %eax
    movl    %eax, %cr0
    
    ret

/* Jump to 64-bit code */
.global longmode_jump
.type longmode_jump, @function
longmode_jump:
    /* Setup stack */
    movl    $0x20000, %esp
    
    /* Load 64-bit GDT */
    lgdt    gdtr64
    
    /* Set data segments */
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss
    
    /* Jump to 64-bit code */
    movl    4(%esp), %edi      /* boot_info */
    movl    8(%esp), %eax      /* entry_point (low 32 bits) */
    xorl    %edx, %edx         /* entry_point (high 32 bits) */
    
    /* Set CS for 64-bit code */
    pushw   $0x08              /* 64-bit code selector */
    pushl   $longmode_entry64
    lret

.code64
.type longmode_entry64, @function
longmode_entry64:
    /* Setup 64-bit stack */
    movq    $0x20000, %rsp
    
    /* Call kernel entry */
    movq    %rdi, %rsi          /* boot_info */
    call    *%rax               /* entry_point */
    
    /* Should never reach here */
    cli
halt:
    hlt
    jmp     halt

/* 64-bit GDT */
.section .rodata
.align 16
.type gdt64, @object
gdt64:
    .quad 0x0000000000000000  /* Null descriptor */
    .quad 0x00209A0000000000  /* Code (64-bit) */
    .quad 0x0000920000000000  /* Data */
    .quad 0x00009A0000000000  /* Code (32-bit compatibility) */

.type gdtr64, @object
gdtr64:
    .word gdt64_end - gdt64 - 1
    .quad gdt64

gdt64_end: