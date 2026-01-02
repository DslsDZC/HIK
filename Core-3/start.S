/*
 * HIK Core-3 Application Startup Code
 * 
 * This is the entry point for Core-3 applications.
 * It sets up the stack and calls the application initialization.
 */

.section .text
.global _app_entry
.type _app_entry, @function

_app_entry:
    /* Disable interrupts */
    cli

    /* Setup stack pointer */
    /* Stack is defined in linker script */
    mov $stack_top, %rsp

    /* Clear direction flag */
    cld

    /* Save registers that might be used by Core-0 */
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15

    /* Core-0 passes process_info in rdi */
    /* Call core3_init */
    mov %rdi, %rdi      /* process_info in rdi */
    call core3_init

    /* Check if initialization succeeded */
    test %rax, %rax
    jz app_init_ok

    /* Initialization failed, exit */
    mov $1, %rdi        /* Exit code 1 */
    call exit

app_init_ok:
    /* Restore registers */
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx

    /* Enable interrupts */
    sti

    /* Get argc and argv from process_info */
    mov g_process_info, %rax
    mov 0x80(%rax), %rdi      /* argc at offset 0x80 */
    mov 0x88(%rax), %rsi      /* argv at offset 0x88 */

    /* Call application main */
    call core3_main

    /* Application returned, exit with return value */
    mov %rax, %rdi
    call exit

.section .bss
.global g_process_info
g_process_info:
    .quad 0

.section .stack
.global stack_top
stack_top:
    .skip 0x10000  /* 64KB stack space */