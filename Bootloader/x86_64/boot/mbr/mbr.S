; HIK BIOS Bootloader - Master Boot Record (MBR)
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

    ; Save boot drive
    mov [drive], dl

    ; Print message
    mov si, msg
    call print_string

    ; Load VBR from first partition
    mov ax, 0x0201
    mov cx, 0x0001
    mov dh, 0
    mov dl, [drive]
    mov bx, 0x7C00
    int 0x13

    jc error

    ; Jump to VBR
    jmp 0x7C00

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

drive db 0
msg db 'HIK Bootloader', 0x0D, 0x0A, 0
err_msg db 'Boot error', 0x0D, 0x0A, 0

; Partition table
times 0x1BE-($-$$) db 0
times 16 db 0
times 16 db 0
times 16 db 0
times 16 db 0

; Signature
dw 0xAA55