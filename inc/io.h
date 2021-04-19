#pragma once

#include <stdint.h>

#define insl(port, buffer, count) \
         __asm__ ("cld; rep; insl" :: "D" (buffer), "d" (port), "c" (count))

#define invlpg(addr) __asm__("invlpg (%%eax)"::"a"(addr))

static __always_inline void outl(uint16_t port, uint32_t val) {
    __asm__("outl %%eax,%%dx"::"a"(val),"d"(port));
}
static __always_inline uint32_t inl(uint16_t port) { 
    uint32_t __ret; 
    __asm__("inl %%dx,%%eax":"=a"(__ret):"d"(port)); 
    return __ret; 
} 

static __always_inline void outportb(uint16_t port, uint8_t val) { 
    __asm__("outb %%al,%%dx"::"a"(val),"d"(port)); 
} 

static __always_inline uint8_t inportb(uint16_t port) { 
    uint8_t __ret; 
    __asm__("inb %%dx,%%al":"=a"(__ret):"d"(port)); 
    return __ret; 
} 