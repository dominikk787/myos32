#pragma once

#include <stdint.h>

#define insd(port, buffer, count) \
         __asm__ ("cld\n rep\n insd" :: "D" (buffer), "d" (port), "c" (count))

#define invlpg(addr) __asm__("invlpg [eax]"::"a"(addr))

static void outl(uint16_t port, uint32_t val) {
    __asm__("out dx,eax"::"a"(val),"d"(port));
}
static uint32_t inl(uint16_t port) { 
    uint32_t __ret; 
    __asm__("in eax,dx":"=a"(__ret):"d"(port)); 
    return __ret; 
} 

static void outportb(uint16_t port, uint8_t val) { 
    __asm__("out dx,al"::"a"(val),"d"(port)); 
} 

static uint8_t inportb(uint16_t port) { 
    uint8_t __ret; 
    __asm__("in al,dx":"=a"(__ret):"d"(port)); 
    return __ret; 
} 