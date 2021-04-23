#include "arch/intr.h" 
#include "arch/io.h"
#include "kernel.h"

static unsigned int cached_irq_mask = 0xffff; 

#define __byte(x,y)       (((unsigned char *)&(y))[x]) 
#define cached_21      (__byte(0,cached_irq_mask)) 
#define cached_A1      (__byte(1,cached_irq_mask)) 

static void set_gate(uint8_t n, uint8_t dpl, uint8_t type, uint32_t addr) {
    idt[n].zero = 0;
    idt[n].type_attr = (addr != 0 ? 1 << 7 : 0) | dpl << 5 | type;
    idt[n].selector = 0x8;
    idt[n].offset_1 = addr & 0xFFFF;
    idt[n].offset_2 = (addr >> 16) & 0xFFFF;
}

void set_intr_gate(uint8_t n, void *addr) { 
    set_gate(n, 0, 14, (uint32_t)addr);
} 

void set_trap_gate(uint8_t n, void *addr) { 
    set_gate(n, 0, 15, (uint32_t)addr);
} 

void set_system_gate(uint8_t n, void *addr) { 
    set_gate(n, 3, 15, (uint32_t)addr);
}

void disable_irq(uint8_t irq) { 
    uint16_t mask = 1 << irq; 
    cached_irq_mask |= mask; 
    if (irq & 8) { 
        outportb(0xA1, cached_A1); 
    } else { 
        outportb(0x21, cached_21); 
    } 
} 

void enable_irq(uint8_t irq) { 
    uint16_t mask = ~(1 << irq); 
    cached_irq_mask &= mask; 
    if (irq & 8) { 
        outportb(0xA1, cached_A1); 
    } else { 
        outportb(0x21, cached_21); 
    } 
}

__attribute__ ((weak)) void do_irq0() {}
__attribute__ ((weak)) void do_irq1() {}
__attribute__ ((weak)) void do_irq2() {}
__attribute__ ((weak)) void do_irq3() {}
__attribute__ ((weak)) void do_irq4() {}
__attribute__ ((weak)) void do_irq5() {}
__attribute__ ((weak)) void do_irq6() {}
__attribute__ ((weak)) void do_irq7() {}
__attribute__ ((weak)) void do_irq8() {}
__attribute__ ((weak)) void do_irq9() {}
__attribute__ ((weak)) void do_irq10() {}
__attribute__ ((weak)) void do_irq11() {}
__attribute__ ((weak)) void do_irq12() {}
__attribute__ ((weak)) void do_irq13() {}
__attribute__ ((weak)) void do_irq14() {}
__attribute__ ((weak)) void do_irq15() {}

__attribute__ ((weak)) void do_exc0() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'a'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc1() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'b'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc2() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'c'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc3() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'd'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc4() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'e'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc5() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'f'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc6() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'g'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc7() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'h'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc8() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'i'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc9() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'j'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc10() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'm'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc11() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'n'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc12() {((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'o'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc13(uint32_t arg) {kprint("gp %08X\n", arg);  ((uint16_t*)0xC00B8000)[0] = 0x0F00 | 'p'; while(1) asm("hlt;");}
__attribute__ ((weak)) void do_exc14(uint32_t arg) {kprint("pf %08X\n", arg); ((uint16_t*)0xC00B8000)[0] = 0x0F00 | '@'; while(1) asm("hlt;");}