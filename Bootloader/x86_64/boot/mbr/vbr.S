; HIK BIOS Bootloader - Volume Boot Record (VBR)
; Simplified version for ISO boot

BITS 16
ORG 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Print loading message
    mov si, msg
    call print_string

    ; Load stage 2 from disk (simplified)
    mov ax, 0x0208
    mov cx, 0x0010
    mov dh, 0
    mov bx, 0x1000
    int 0x13

    jc error

    ; Jump to stage 2
    jmp 0x1000

error:
    mov si, err_msg
    call print_string
    hlt
    jmp $

print_string:
    lodsb
    or al, al
    jz done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp print_string
done:
    ret

msg db 'Loading HIK...', 0x0D, 0x0A, 0
err_msg db 'Load failed', 0x0D, 0x0A, 0

; Pad to 512 bytes
times 510-($-$$) db 0
dw 0xAA55