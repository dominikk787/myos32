bits 32

section .text
global get_cmos

get_cmos:
    cli
    mov eax, [esp + 4]  ; proper distance for 1st arg.
    mov edx, 0x70
    out dx, al

    mov edx, 0x71
    in al, dx
    sti
    ret