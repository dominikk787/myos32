#pragma once

#include <stdint.h>

#define insd(port, buffer, count) \
         __asm__ ("cld\n rep\n insd" :: "D" (buffer), "d" (port), "c" (count))

#define invlpg(addr) __asm__("invlpg [eax]"::"a"(addr))

#define outl(port, val) __asm__("out dx,eax"::"a"(val),"d"(port))
#define outb(port, val) __asm__("out dx,al"::"a"(val),"d"(port))


static inline uint32_t inl(uint16_t port) { 
    uint32_t __ret; 
    __asm__("in eax,dx":"=a"(__ret):"d"(port)); 
    return __ret; 
} 
static inline uint8_t inb(uint16_t port) { 
    uint8_t __ret; 
    __asm__("in al,dx":"=a"(__ret):"d"(port)); 
    return __ret; 
} 

static inline void write_serial(char a) {
loop_thr:
    asm goto("in al,dx\ntest al,cl\njz %l2"::"d"(0x3F8 + 5),"c"(0x20):"al":loop_thr);
//    while ((inb(0x3F8 + 5) & 0x20) == 0);
   outb(0x3F8, a);
//    while ((inb(0x3F8 + 5) & 0x40) == 0);
}