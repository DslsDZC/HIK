/*
 * HIK Core-1 Service Startup Code
 * 
 * This is the entry point for Core-1 services.
 * It sets up the stack and calls the service initialization.
 */

.section .text
.global _service_entry
.type _service_entry, @function

_service_entry:
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

    /* Core-0 passes service_info in rdi and core0_api in rsi */
    /* Call core1_init */
    mov %rdi, %rdi      /* service_info in rdi */
    mov %rsi, %rsi      /* core0_api in rsi */
    call core1_init

    /* Check if initialization succeeded */
    test %rax, %rax
    jz service_init_ok

    /* Initialization failed, halt */
    cli
    hlt
    jmp .

service_init_ok:
    /* Restore registers */
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx

    /* Enable interrupts */
    sti

    /* Call service main */
    call core1_main

    /* Service returned, halt */
    cli
    hlt
    jmp .

.section .stack
.global stack_top
stack_top:
    .skip 0x10000  /* 64KB stack space */