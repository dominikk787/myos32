bits 32

global low_start, gdt, idt, pd 
extern start_kernel, reroute_irqs
extern _code, _edata, _end

section .multiboot.data
mboot: 
    dd 0xE85250D6
    dd 0
    dd mboot_end - mboot
    dd - (0xE85250D6 + 0 + (mboot_end - mboot))

    dw 2
    dw 1
    dd 24
    dd mboot
    dd _code
    dd _edata
    dd _end

    dw 3
    dw 1
    dd 12
    dd low_start
    dd 0

    dw 5
    dw 1
    dd 20
    dd 1024
    dd 768
    dd 16
    dd 0

    dw 0
    dw 0
    dd 8
mboot_end:

section .multiboot.text
low_start: 
    cli 
    mov edx, eax
    mov esi, ebx
    add esi, 0xC0000000
    ; paging
    mov dword [pd - 0xC0000000], pt - 0xC0000000 + 2 + 1
    mov dword [pd - 0xC0000000 + 4], pt - 0xC0000000 + 0x1000 + 2 + 1
    mov dword [pd - 0xC0000000 + 0xC00], pt - 0xC0000000 + 2 + 1
    mov dword [pd - 0xC0000000 + 0xC04], pt - 0xC0000000 + 0x1000 + 2 + 1
    mov dword [pd - 0xC0000000 + 0xFFC], pd - 0xC0000000 + 2 + 1
    mov eax, 0
    mov ebx, pt - 0xC0000000
.loop_pt01:
    mov ecx, eax
    add ecx, 3
    mov dword [ebx], ecx
    add ebx, 4
    add eax, 0x1000
    test eax, 0x800000
    jz .loop_pt01

    mov eax, pd - 0xC0000000
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000001
    mov cr0, eax
    jmp high_start
    hlt

section .text
high_start:
    mov dword [pd], 0
    mov dword [pd + 4], 0
    mov esp, stack_top 
    push dword 0 
    push dword 0 
    push dword 0 
    push dword .stop
    push esi
    push edx
    mov dword [gdt + 0x8], 0x0000FFFF
    mov dword [gdt + 0xC], 0x00CF9A00
    mov dword [gdt + 0x10], 0x0000FFFF
    mov dword [gdt + 0x14], 0x00CF9200
    lgdt [gdt.pointer]
    jmp 0x08:.reload_CS
.reload_CS:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    lidt [idt.pointer]
    call reroute_irqs

    mov eax, cr0
    and ax, 0xFFFB		;clear coprocessor emulation CR0.EM
    or ax, 0x22			;set coprocessor monitoring  CR0.MP & CR0.NE
    mov cr0, eax
    mov eax, cr4
    or ax, 3 << 9		;set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    mov cr4, eax
    finit

    call start_kernel
.stop: 
    hlt
    jmp .stop

section .bss
align 4096
stack_bottom:
    resd 16 * 1024
stack_top:
gdt: 
    resq 256
idt: 
    resq 256

section .bss
align 4096
pd:
    resd 1024
pt:
    resd 1024 * 2

section .rodata
gdt.pointer:
    dw 256*8-1 
    dd gdt
idt.pointer:
    dw 256*8-1 
    dd idt