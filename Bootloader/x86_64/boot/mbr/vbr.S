/*
 * HIK BIOS Bootloader - Volume Boot Record (VBR)
 * 
 * This is the second stage that is loaded by MBR to 0x7C00.
 * It performs the following tasks:
 * 1. Initialize protected mode environment
 * 2. Load stage 2 bootloader from file system
 * 3. Jump to stage 2
 * 
 * This code is loaded to the VBR of the active partition.
 */

.section .text
.global _start
.code16

_start:
    cli                     /* Disable interrupts */
    
    /* Save drive number (from MBR) */
    movb 0x6000, %dl
    
    /* Setup segment registers */
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7C00, %sp       /* Stack at top of VBR location */
    
    /* Enable A20 (already done by MBR, but ensure it's enabled) */
    call check_a20
    
    /* Load GDT */
    lgdt gdtr
    
    /* Enable protected mode */
    movw %cr0, %ax
    orb $1, %al             /* Set PE bit */
    movw %ax, %cr0
    
    /* Far jump to protected mode code */
    ljmp $0x08, $protected_mode

.code32
protected_mode:
    /* Setup segment registers for protected mode */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    
    /* Setup stack */
    movl $0x90000, %esp
    
    /* Clear screen */
    call clear_screen
    
    /* Print "Loading HIK..." message */
    movl $loading_msg, %esi
    movl $0xB8000, %edi
    movl $14, %ecx
    call print_string
    
    /* Load stage 2 bootloader */
    call load_stage2
    
    /* Check if load was successful */
    testl %eax, %eax
    jnz jump_to_stage2
    
    /* Load failed, print error */
    movl $0xB8000 + 160, %edi  /* Next line */
    movl $error_msg, %esi
    movl $16, %ecx
    call print_string
halt:
    hlt
    jmp halt

jump_to_stage2:
    /* Jump to stage 2 at 0x10000 */
    ljmp $0x08, $0x10000

/*
 * Check if A20 is enabled
 */
check_a20:
    pushl %eax
    pushl %ecx
    
    /* Test A20 using memory at 0x0000 and 0x100000 */
    movl $0, %eax
    movl %eax, 0x0000
    movl $0xFF, %eax
    movl %eax, 0x100000
    
    movl 0x0000, %eax
    cmpl $0, %eax
    je a20_disabled
    
    /* A20 is enabled */
    popl %ecx
    popl %eax
    ret

a20_disabled:
    /* A20 is disabled, this shouldn't happen */
    /* Try to enable it again */
    popl %ecx
    popl %eax
    /* Fall through to halt */
    hlt
    jmp halt

/*
 * Load stage 2 bootloader
 * Returns: %eax = 1 on success, 0 on failure
 */
load_stage2:
    /* For now, load stage 2 from fixed LBA offset */
    /* In a real implementation, this would parse the file system */
    
    /* Assume stage 2 starts at LBA 2048 (1MB offset) */
    movl $2048, %eax         /* LBA to read */
    movl $0x10000, %ebx      /* Destination address */
    movl $64, %ecx           /* Number of sectors (32KB) */
    call read_disk
    
    testl %eax, %eax
    jnz load_success
    
    /* Read failed */
    xorl %eax, %eax
    ret

load_success:
    movl $1, %eax
    ret

/*
 * Read disk sectors using BIOS INT 13h extensions
 * %eax = LBA to read
 * %ebx = destination address
 * %ecx = number of sectors
 * Returns: %eax = sectors read, 0 on error
 */
read_disk:
    /* Setup disk address packet */
    pushl %ebp
    movl %esp, %ebp
    
    /* Build DAP at 0x5000 */
    movl $0x5000, %edi
    
    movb $16, (%edi)           /* Packet size */
    movb %cl, 1(%edi)          /* Number of sectors to read */
    movw %bx, 2(%edi)          /* Offset to buffer */
    movw $0, 4(%edi)           /* Segment (0 in protected mode) */
    movl %eax, 6(%edi)         /* LBA (low 32 bits) */
    movl $0, 10(%edi)          /* LBA (high 32 bits) */
    
    /* Load drive number */
    movb 0x6000, %dl
    
    /* Switch to real mode temporarily for BIOS call */
    call enter_real_mode
    
    /* Call BIOS INT 13h, AH=42h (Extended Read) */
    movw $0x4200, %ax
    movw $0x5000, %si
    int $0x13
    
    /* Switch back to protected mode */
    call enter_protected_mode
    
    /* Check for error */
    jc read_error
    
    /* Success, return sectors read */
    movl %ecx, %eax
    movl %ebp, %esp
    popl %ebp
    ret

read_error:
    xorl %eax, %eax
    movl %ebp, %esp
    popl %ebp
    ret

/*
 * Enter real mode for BIOS calls
 */
enter_real_mode:
    /* Save protected mode state */
    pushal
    pushw %ds
    pushw %es
    pushw %fs
    pushw %gs
    
    /* Disable paging */
    movl %cr0, %eax
    andl $0x7FFFFFFF, %eax
    movl %eax, %cr0
    
    /* Load real mode segment */
    movw $0, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    /* Disable protected mode */
    movw %cr0, %ax
    andb $0xFE, %al           /* Clear PE bit */
    movw %ax, %cr0
    
    /* Far jump to real mode */
    ljmp $0, $real_mode

.code16
real_mode:
    /* Setup real mode segments */
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7C00, %sp
    
    /* Enable interrupts for BIOS calls */
    sti
    ret

/*
 * Enter protected mode after BIOS calls
 */
enter_protected_mode:
    cli                     /* Disable interrupts */
    
    /* Load GDT */
    lgdt gdtr
    
    /* Enable protected mode */
    movw %cr0, %ax
    orb $1, %al             /* Set PE bit */
    movw %ax, %cr0
    
    /* Far jump to protected mode code */
    ljmp $0x08, $protected_mode2

.code32
protected_mode2:
    /* Setup segment registers for protected mode */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    
    /* Restore protected mode state */
    popw %gs
    popw %fs
    popw %es
    popw %ds
    popal
    ret

/*
 * Clear screen
 */
clear_screen:
    movl $0xB8000, %edi
    movl $80*25*2, %ecx        /* Screen size */
    movw $0x0720, %ax          /* Space with gray-on-black attribute */
    rep stosw
    ret

/*
 * Print string to VGA text mode
 * %esi = pointer to string
 * %edi = destination address (0xB8000)
 * %ecx = length
 */
print_string:
    movl $0x0F, %edx           /* White on black attribute */
    
print_loop:
    lodsb                      /* Load byte from %esi */
    stosb                      /* Store to %edi */
    movb %dl, %al
    stosb                      /* Store attribute */
    loop print_loop
    ret

/*
 * Data
 */
loading_msg:
    .ascii "Loading HIK..."
    
error_msg:
    .ascii "Load failed!"

/*
 * GDT (Global Descriptor Table)
 */
.align 4
gdt:
    /* Null descriptor */
    .word 0, 0
    .byte 0, 0, 0, 0
    
    /* Code descriptor (32-bit, ring 0) */
    .word 0xFFFF, 0
    .byte 0, 0x9A, 0xCF, 0
    
    /* Data descriptor (32-bit, ring 0) */
    .word 0xFFFF, 0
    .byte 0, 0x92, 0xCF, 0

gdt_end:

/*
 * GDTR (GDT Register)
 */
gdtr:
    .word gdt_end - gdt - 1
    .long gdt

/*
 * Pad to 512 bytes (VBR size)
 */
.org 510
    .word 0xAA55              /* VBR signature */