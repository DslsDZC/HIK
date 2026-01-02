/*
 * HIK Core-0 Multiboot Header and Entry Point
 * 
 * This file contains the Multiboot header and the kernel entry point.
 */

/* Multiboot header magic number */
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
#define MULTIBOOT_HEADER_FLAGS 0
#define MULTIBOOT_HEADER_CHECKSUM -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

.section .multiboot, "a"
.align 4
.global multiboot_header
.type multiboot_header, @object
multiboot_header:
    .long MULTIBOOT_HEADER_MAGIC
    .long MULTIBOOT_HEADER_FLAGS
    .long MULTIBOOT_HEADER_CHECKSUM
    .long 0  /* header_addr */
    .long 0  /* load_addr */
    .long 0  /* load_end_addr */
    .long 0  /* bss_end_addr */
    .long 0  /* entry_addr */
    .long 0  /* mode_type */
    .long 0  /* width */
    .long 0  /* height */
    .long 0  /* depth */

.section .text

.global _start
.type _start, @function
_start:
    /* Disable interrupts */
    cli

    /* Setup stack (use 64-bit stack, 16-byte aligned) */
    movq $0x20000, %rsp
    andq $-16, %rsp  /* Align to 16 bytes */

    /* Clear BSS segment */
    /* BSS starts at 0x105300, ends at 0x338b80 */
    movq $0x105300, %rdi
    movq $0x338b80, %rcx
    subq %rdi, %rcx
    xorq %rax, %rax
    rep stosb

    /* Write debug message to VGA */
    movq $0xB8000, %rdi
    movw $0x0F54, %ax  /* 'T' in white */
    stosw
    movw $0x0F45, %ax  /* 'E' */
    stosw
    movw $0x0F53, %ax  /* 'S' */
    stosw
    movw $0x0F54, %ax  /* 'T' */
    stosw

    /* Write 'B' before call */
    movw $0x0F42, %ax
    stosw

    /* Save rdi (VGA pointer) for later use */
    movq %rdi, %r8

    /* Call kernel initialization with multiboot info pointer in rdi */
    /* In System V AMD64 ABI, first argument is in rdi */
    movq %rax, %rdi
    call kernel_init

    /* Write 'A' after call */
    movw $0x0F41, %ax
    stosw

    /* If kernel_init returns, halt */
    cli
halt:
    hlt
    jmp halt

.size _start, . - _start