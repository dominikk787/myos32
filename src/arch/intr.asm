section .text
bits 32

global reroute_irqs, irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7, irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
extern do_irq0, do_irq1, do_irq2, do_irq3, do_irq4, do_irq5, do_irq6, do_irq7, do_irq8, do_irq9, do_irq10, do_irq11, do_irq12, do_irq13, do_irq14, do_irq15
irqm_table:
    dd do_irq0, do_irq1, do_irq2, do_irq3, do_irq4, do_irq5, do_irq6, do_irq7
irqs_table:
    dd do_irq8, do_irq9, do_irq10, do_irq11, do_irq12, do_irq13, do_irq14, do_irq15

global exc0, exc1, exc2, exc3, exc4, exc5, exc6, exc7, exc8, exc9, exc10, exc11, exc12, exc13, exc14, exc16
extern do_exc0, do_exc1, do_exc2, do_exc3, do_exc4, do_exc5, do_exc6, do_exc7, do_exc8, do_exc9, do_exc10, do_exc11, do_exc12, do_exc13, do_exc14, do_exc16
exception_table: 
dd do_exc0, do_exc1, do_exc2, do_exc3, do_exc4, do_exc5, do_exc6, do_exc7 
dd do_exc8, do_exc9, do_exc10, do_exc11, do_exc12, do_exc13, do_exc14, do_exc16

reroute_irqs: 
    cli 

    mov     al,0x11 
    out     0x20,al 
    out     0xEB,al 
    out     0xA0,al 
    out     0xEB,al 

    mov     al,0x20 
    out     0x21,al 
    out     0xEB,al 

    add     al,0x8 
    out     0xA1,al 
    out     0xEB,al 

    mov     al,0x04 
    out     0x21,al 
    out     0xEB,al 
    shr     al,1 
    out     0xA1,al 
    out     0xEB,al 
    shr     al,1 
    out     0x21,al 
    out     0xEB,al 
    out     0xA1,al 
    out     0xEB,al 

    cli 
    mov     al,0xFF
    out     0xA1,al 
    out     0x21,al 

    ret

irq0: 
    pusha 
    mov ebx, 0
    jmp irqm

irq1: 
    pusha 
    mov ebx, 1
    jmp irqm

irq2: 
    pusha 
    mov ebx, 2
    jmp irqm

irq3: 
    pusha 
    mov ebx, 3
    jmp irqm

irq4: 
    pusha 
    mov ebx, 4
    jmp irqm

irq5: 
    pusha 
    mov ebx, 5
    jmp irqm

irq6: 
    pusha 
    mov ebx, 6
    jmp irqm

irq7: 
    pusha 
    mov ebx, 7
    jmp irqm

irq8: 
    pusha 
    mov ebx, 0
    jmp irqs

irq9:  
    pusha 
    mov ebx, 1
    jmp irqs

irq10:  
    pusha 
    mov ebx, 2
    jmp irqs

irq11:  
    pusha 
    mov ebx, 3
    jmp irqs

irq12:  
    pusha 
    mov ebx, 4
    jmp irqs

irq13:  
    pusha 
    mov ebx, 5
    jmp irqs

irq14:  
    pusha 
    mov ebx, 6
    jmp irqs

irq15:  
    pusha 
    mov ebx, 7
    jmp irqs

irqm:
    push gs 
    push fs 
    push es 
    push ds 
    mov ax,0x10 
    mov ds,ax 
    mov es,ax 
    mov al,bl
    add al,0x60
    out 0x20,al 
    ; mov al, 0x20
    ; out 0x20,al
    call dword [irqm_table+ebx*4] 
    pop ds 
    pop es 
    pop fs 
    pop gs 
    popa
    iret

irqs:
    push gs 
    push fs 
    push es 
    push ds 
    mov ax,0x10 
    mov ds,ax 
    mov es,ax 
    mov al,bl
    add al,0x60
    out 0xA0,al 
    mov al,0x62
    out 0x20,al
    ; mov al, 0x20
    ; out 0xA0,al
    ; out 0x20,al
    call dword [irqs_table+ebx*4] 
    pop ds 
    pop es 
    pop fs 
    pop gs 
    popa
    iret

exc0: 
   push dword 0 
   push dword 0 
   jmp handle_exception 

exc1: 
   push dword 0       
   push dword 1 
   jmp handle_exception 

exc2: 
   push dword 0 
   push dword 2 
   jmp handle_exception 

exc3: 
   push dword 0 
   push dword 3 
   jmp handle_exception 

exc4: 
   push dword 0 
   push dword 4 
   jmp handle_exception 

exc5: 
   push dword 5 
   jmp handle_exception 

exc6: 
   push dword 0 
   push dword 6 
   jmp handle_exception 

exc7: 
   push dword 0 
   push dword 7 
   jmp handle_exception 

exc8: 
   push dword 8 
   jmp handle_exception 

exc9: 
   push dword 0 
   push dword 9 
   jmp handle_exception 

exc10: 
   push dword 10 
   jmp handle_exception 

exc11: 
   push dword 11 
   jmp handle_exception 

exc12: 
   push dword 12 
   jmp handle_exception 

exc13: 
   push dword 13 
   jmp handle_exception 

exc14: 
   push dword 14 
   jmp handle_exception

exc16: 
   push dword 16 
   jmp handle_exception

handle_exception: 
    cli
    xchg eax,[esp] 
    xchg ebx,[esp+4] 
    push gs 
    push fs 
    push es 
    push ds 
    push ebp 
    push edi 
    push esi 
    push edx 
    push ecx 
    push ebx 
    mov ecx,0x10 
    mov ds,cx 
    mov es,cx 
    call dword [exception_table+eax*4] 
    pop eax 
    pop ecx 
    pop edx 
    pop esi 
    pop edi 
    pop ebp 
    pop ds 
    pop es 
    pop fs 
    pop gs 
    pop eax 
    pop ebx 
    iret