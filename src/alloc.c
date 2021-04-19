#include "kernel.h"
#include "alloc.h"
#include "io.h"

uint32_t bitmap[64] = {};
uint32_t __attribute__((aligned(4096))) alloc_pts[1024 * 2] = {};

void * get_physaddr(void * virtualaddr) {
    uint32_t pdindex = (uint32_t)virtualaddr >> 22;
    uint32_t ptindex = (uint32_t)virtualaddr >> 12 & 0x03FF;
    // Here you need to check whether the PD entry is present.

    if(pd[pdindex] & 1 == 0) return 0xFFFFFFFF;
 
    uint32_t *pt = (uint32_t *)(pd[pdindex] & 0xFFFFF000);
    // Here you need to check whether the PT entry is present.
    if(pt[ptindex] & 1 == 0) return 0xFFFFFFFF;
 
    return (void *)((pt[ptindex] & 0xFFFFF000) + ((uint32_t)virtualaddr & 0xFFF));
}

void pt_set_entry(void *pt, uint16_t n, uint32_t page, uint8_t user, uint8_t rw, uint8_t present) {
    uint32_t buf = page & 0xFFFFF000;
    buf |= (user & 1) << 2;
    buf |= (rw & 1) << 1;
    buf |= present & 1;
    ((uint32_t*)pt)[n] = buf;
}
void alloc_free_pages(void *addr, uint32_t num) {
    for(uint32_t n = 0; n < num; n++) {
        uint32_t phys = alloc_pts[(((uint32_t)addr - 0xE0000000) >> 12) + n] & 0xFFFFF000;
        uint32_t bit = (phys - 0x800000) >> 12;
        bitmap[bit >> 5] &= ~(1 << (bit & 0b11111));
        // alloc_pts[(((uint32_t)addr - 0xE0000000) >> 12) + n] = 0;
        printf("free entry %08X at %08X bitmap %08X\n", (((uint32_t)addr - 0xE0000000) >> 12) + n, phys, bitmap[bit >> 5]);
        pt_set_entry(alloc_pts, (((uint32_t)addr - 0xE0000000) >> 12) + n, 0, 0, 0, 0);
        __asm__("invlpg (%%eax)"::"a"((uint32_t)addr + (n << 12)));
    }
}
void *alloc_pages(void *addr, uint32_t num, uint8_t user) {
    for(uint32_t n = 0; n < num; n++) {
        uint32_t free = 0xFFFFFFFF;
        for(uint8_t i = 0; i < 64; i++)
            if(bitmap[i] < 0xFFFFFFFF) {
                free = i;
                break;
            }
        if(free == 0xFFFFFFFF && n > 0)
            alloc_free_pages(addr, n);
        if(free == 0xFFFFFFFF)
            return (void*)0;
        for(uint8_t i = 0; i < 32; i++)
            if((bitmap[free] & (1 << i)) == 0) {
                free = (free * 64) + i;
                break;
            }
        bitmap[free >> 5] |= 1 << (free & 0b11111);
        pt_set_entry(alloc_pts, (((uint32_t)addr - 0xE0000000) >> 12) + n, (free << 12) + 0x800000, user, 1, 1);
        printf("allocated entry %08X at %08X bitmap %08X\n", (((uint32_t)addr - 0xE0000000) >> 12) + n, (free << 12) + 0x800000, bitmap[free >> 5]);
        __asm__("invlpg (%%eax)"::"a"((uint32_t)addr + (n << 12)));
    }
    // __asm__("mov %%cr3,%%eax; mov %%eax,%%cr3":::"eax");
    return addr;
}
void alloc_init() {
    for(uint32_t i = 0; i < 2048; i++) alloc_pts[i] = 0;
    for(uint32_t i = 0; i < 64; i++) bitmap[i] = 0;
    pt_set_entry(pd, 0xE00 >> 2, (uint32_t)alloc_pts - 0xC0000000, 1, 1, 1);
    pt_set_entry(pd, 0xE04 >> 2, (uint32_t)alloc_pts - 0xC0000000 + 4096, 1, 1, 1);
}