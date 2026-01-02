/*
 * HIK BIOS Bootloader - Master Boot Record (MBR)
 * 
 * This is the first stage bootloader that is loaded by BIOS to 0x7C00.
 * It performs the following tasks:
 * 1. Load itself to 0x7C00 (done by BIOS)
 * 2. Relocate itself to 0x6000
 * 3. Switch to protected mode
 * 4. Load VBR from active partition
 * 5. Jump to VBR
 *
 * Total size: 512 bytes (including partition table and signature)
 */

.section .text
.global _start
.code16

_start:
    cli                     /* Disable interrupts */
    
    /* Save boot drive number */
    movb %dl, 0x6000        /* Save drive number at 0x6000 */
    
    /* Relocate MBR from 0x7C00 to 0x6000 */
    movw $0x7C00, %si
    movw $0x6000, %di
    movw $256, %cx          /* 512 bytes / 2 = 256 words */
    rep movsw
    
    /* Jump to relocated code */
    ljmp $0, $relocated

relocated:
    /* Setup segment registers */
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7C00, %sp       /* Stack at top of original MBR location */
    
    /* Enable A20 address line */
    call enable_a20
    
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
    
    /* Clear screen */
    call clear_screen
    
    /* Print "HIK Bootloader" message */
    movl $boot_msg, %esi
    movl $0xB8000, %edi
    movl $12, %ecx         /* Length of message */
    call print_string
    
    /* Find active partition and load its VBR */
    call find_active_partition
    testl %eax, %eax
    jnz load_vbr
    
    /* No active partition found, halt */
    movl $0xB8000, %edi
    movl $error_msg, %esi
    movl $21, %ecx
    call print_string
halt:
    hlt
    jmp halt

load_vbr:
    /* Load VBR from active partition */
    /* %eax contains LBA of partition start */
    
    /* Load VBR to 0x7C00 */
    movl $0x7C00, %ebx     /* Destination address */
    movl $1, %ecx          /* Number of sectors to read */
    call read_disk
    
    /* Check if read was successful */
    testl %eax, %eax
    jnz jump_to_vbr
    
    /* Read failed, halt */
    movl $0xB8000, %edi
    movl $read_error_msg, %esi
    movl $18, %ecx
    call print_string
    hlt
    jmp halt

jump_to_vbr:
    /* Jump to VBR at 0x7C00 */
    ljmp $0x08, $0x7C00

/*
 * Enable A20 address line
 * Returns: nothing
 */
enable_a20:
    /* Method 1: Try keyboard controller */
    call a20_wait1
    movb $0xAD, %al        /* Disable keyboard */
    outb %al, $0x64
    
    call a20_wait1
    movb $0xD0, %al        /* Read output port */
    outb %al, $0x64
    
    call a20_wait2
    inb $0x60, %al         /* Read status */
    
    call a20_wait1
    movb $0xD1, %al        /* Write output port */
    outb %al, $0x64
    
    call a20_wait1
    orb $2, %al            /* Set A20 bit */
    outb %al, $0x60
    
    call a20_wait1
    movb $0xAE, %al        /* Enable keyboard */
    outb %al, $0x64
    
    call a20_wait1
    ret

a20_wait1:
    inb $0x64, %al
    testb $2, %al
    jnz a20_wait1
    ret

a20_wait2:
    inb $0x64, %al
    testb $1, %al
    jz a20_wait2
    ret

/*
 * Find active partition
 * Returns: %eax = LBA of active partition start, 0 if not found
 */
find_active_partition:
    movl $0x7C00 + 0x1BE, %esi  /* Start of partition table */
    movl $4, %ecx               /* 4 partitions */
    
find_loop:
    /* Check if partition is active (bootable) */
    testb $0x80, (%esi)
    jnz found_active
    
    addl $16, %esi             /* Next partition entry (16 bytes) */
    loop find_loop
    
    /* No active partition found */
    xorl %eax, %eax
    ret

found_active:
    /* Get LBA of partition start */
    movl 8(%esi), %eax
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
    movw $0x6000, 2(%edi)      /* Offset to buffer (relocated MBR) */
    movw $0, 4(%edi)           /* Segment */
    movl %eax, 6(%edi)         /* LBA (low 32 bits) */
    movl $0, 10(%edi)          /* LBA (high 32 bits) */
    
    /* Load drive number */
    movb 0x6000, %dl
    
    /* Call BIOS INT 13h, AH=42h (Extended Read) */
    movw $0x4200, %ax
    movw $0x5000, %si
    int $0x13
    
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
boot_msg:
    .ascii "HIK Bootloader"
    
error_msg:
    .ascii "No active partition"
    
read_error_msg:
    .ascii "Failed to read disk"

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
 * Partition table (filled by build system)
 * Starts at offset 0x1BE (446 bytes)
 */
.org 0x1BE

/* Partition 1 */
partition1:
    .byte 0x80                /* Bootable flag */
    .byte 0, 0, 0            /* CHS start */
    .byte 0x07                /* Partition type (NTFS/exFAT) */
    .byte 0, 0, 0            /* CHS end */
    .long 0                   /* LBA start (filled by build system) */
    .long 0                   /* Size in sectors (filled by build system) */

/* Partition 2 */
partition2:
    .byte 0
    .byte 0, 0, 0
    .byte 0
    .byte 0, 0, 0
    .long 0
    .long 0

/* Partition 3 */
partition3:
    .byte 0
    .byte 0, 0, 0
    .byte 0
    .byte 0, 0, 0
    .long 0
    .long 0

/* Partition 4 */
partition4:
    .byte 0
    .byte 0, 0, 0
    .byte 0
    .byte 0, 0, 0
    .long 0
    .long 0

/*
 * MBR signature (0x55AA)
 */
.org 0x1FE
    .word 0xAA55