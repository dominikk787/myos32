#pragma once

#include <stdint.h>

typedef struct desc_struct { 
    uint32_t a,b; 
} desc_table[256]; 

typedef struct IDTDescr {
   uint16_t offset_1; // offset bits 0..15
   uint16_t selector; // a code segment selector in GDT or LDT
   uint8_t zero;      // unused, set to 0
   uint8_t type_attr; // type and attributes, see below
   uint16_t offset_2; // offset bits 16..31
} idt_table[256];

extern desc_table gdt;
extern idt_table idt; 

void set_intr_gate(uint8_t n, void (*func)(void)); 
void set_trap_gate(uint8_t n, void (*func)(void)); 
void set_system_gate(uint8_t n, void (*func)(void));

void disable_irq(uint8_t irq);
void enable_irq(uint8_t irq);

void irq0(void);
void irq1(void);
void irq2(void);
void irq3(void);
void irq4(void);
void irq5(void);
void irq6(void);
void irq7(void);
void irq8(void);
void irq9(void);
void irq10(void);
void irq11(void);
void irq12(void);
void irq13(void);
void irq14(void);
void irq15(void);

void exc0(void);
void exc1(void);
void exc2(void);
void exc3(void);
void exc4(void);
void exc5(void);
void exc6(void);
void exc7(void);
void exc8(void);
void exc9(void);
void exc10(void);
void exc11(void);
void exc12(void);
void exc13(void);
void exc14(void);
void exc16(void);