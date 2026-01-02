; HIK BIOS Bootloader - Stage 2 Startup (Simplified)
;
; This is the entry point for stage 2 bootloader.
; It sets up the stack and calls the C main function.

section .text
global _start
extern stage2_main

_start:
    ; Setup stack
    mov esp, 0x90000

    ; Clear BSS
    mov edi, _bss_start
    mov ecx, _bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    ; Call C main function
    call stage2_main

    ; Should never reach here
    hlt
    jmp _start

; BSS section
section .bss
align 4
global _bss_start
_bss_start:
    resb 4096
global _bss_end
_bss_end: