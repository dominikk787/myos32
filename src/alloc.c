#include "arch/io.h"
#include "drivers.h"
#include "kernel.h"

#include <stdio.h>

uint32_t get_physaddr(uint32_t virtualaddr) {
    uint32_t pdindex = virtualaddr >> 22;
    uint32_t ptindex = virtualaddr >> 12 & 0x03FF;

    uint32_t *pd = (uint32_t *)0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    if((pd[pdindex] & 1) == 0) return 0xFFFFFFFF;

    uint32_t *pt = ((uint32_t *)0xFFC00000) + (0x400 * pdindex);
    // Here you need to check whether the PT entry is present.
    if((pt[ptindex] & 1) == 0) return 0xFFFFFFFF;

    return (pt[ptindex] & (uint32_t)~0xFFF) + (virtualaddr & 0xFFF);
}
void pt_set_entry(void *pt, uint16_t n, uint32_t page, uint8_t user, uint8_t rw,
                  uint8_t present) {
    uint32_t buf = page & 0xFFFFF000;
    buf |= (user & 1) << 2;
    buf |= (rw & 1) << 1;
    buf |= present & 1;
    // printf("pt %03X\n", n << 2);
    ((uint32_t *)pt)[n] = buf;
}

static void set_state(drv_mem_t *drv, uint8_t state) {
    drv_pagealloc_data_t *data = drv->drv_data;
    for(uint32_t i = 0; i < data->numpts; i++) {
        // printf("pd %u\n", i);
        pt_set_entry((void *)0xFFFFF000,
                     (uint16_t)((data->addr_start >> 22) + i),
                     get_physaddr((uint32_t)data->pts[i]),
                     (data->flags & MEM_FLAG_USER) == MEM_FLAG_USER,
                     (data->flags & MEM_FLAG_RO) != MEM_FLAG_RO, state == 1);
    }
    data->state = (state == 1);
}
static void free_page(drv_mem_t *drv, void *addr, uint32_t size) {
    drv_pagealloc_data_t *data = drv->drv_data;
    for(uint32_t n = 0; n < size; n++) {
        uint32_t pt_entry = (((uint32_t)addr - data->addr_start) >> 12) + n;
        uint32_t phys
          = data->pts[pt_entry >> 10][pt_entry & 0x3FF] & 0xFFFFF000;
        if(phys < 0x800000
           || phys >= ((data->total_range * 4096 * 32) + 0x800000))
            continue;
        uint32_t bit = (phys - 0x800000) >> 12;
        data->map[bit >> 5] &= (uint16_t) ~(1 << (bit & 31));
        // printf("free entry %08X at %08X bitmap %08X\n", (((uint32_t)addr -
        // data->addr_start) >> 12) + n, phys, data->map[bit >> 5]);
        pt_set_entry(data->pts[pt_entry >> 10], (uint16_t)(pt_entry & 0x3FF), 0,
                     0, 0, 0);
        invlpg((uint32_t)addr + (n << 12));
    }
}
static void *alloc_page(drv_mem_t *drv, void *addr, uint32_t size) {
    drv_pagealloc_data_t *data = drv->drv_data;
    for(uint32_t n = 0; n < size; n++) {
        uint32_t free = 0xFFFFFFFF;
        for(uint8_t i = 0; i < (data->total_range / 32); i++)
            if(data->map[i] < 0xFFFFFFFF) {
                free = i;
                break;
            }
        if(free == 0xFFFFFFFF && n > 0) free_page(drv, addr, n);
        if(free == 0xFFFFFFFF) return (void *)0;
        for(uint8_t i = 0; i < 32; i++)
            if((data->map[free] & (1 << i)) == 0) {
                free = (free << 5) + i;
                break;
            }
        data->map[free >> 5] |= 1 << (free & 31);
        uint32_t pt_entry = (((uint32_t)addr - data->addr_start) >> 12) + n;
        pt_set_entry(data->pts[pt_entry >> 10], (uint16_t)(pt_entry & 0x3FF),
                     (free << 12) + 0x800000,
                     (data->flags & MEM_FLAG_USER) == MEM_FLAG_USER,
                     (data->flags & MEM_FLAG_RO) != MEM_FLAG_RO, 1);
        // printf("allocated entry %08X at %08X bitmap %08X\n", (((uint32_t)addr
        // - 0xE0000000) >> 12) + n, (free << 12) + 0x800000, data->map[free >>
        // 5]);
        invlpg((uint32_t)addr + (n << 12));
    }
    return addr;
}

void drv_pagealloc_init(drv_mem_t *drv) {
    drv_pagealloc_data_t *data = drv->drv_data;
    data->state = 0;
    for(uint32_t i = 0; i < data->numpts; i++) {
        for(uint32_t j = 0; j < 1024; j++) data->pts[i][j] = 0;
    }
    drv->set_state = set_state;
    drv->alloc = alloc_page;
    drv->free = free_page;
    drv->unit = 4096;
}