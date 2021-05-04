#pragma once

#include <stdint.h>

enum {
    MEM_FLAG_USER = 0x1,
    MEM_FLAG_RO = 0x2
};

struct _driver_mem_t;

typedef struct _driver_mem_t {
    void (*free)(struct _driver_mem_t *drv, void *addr, uint32_t size);
    void *(*alloc)(struct _driver_mem_t *drv, void* addr, uint32_t size);
    void (*set_state)(struct _driver_mem_t *drv, uint8_t state);
    uint32_t unit;
    void *drv_data;
    uint32_t user_data;
} drv_mem_t;

typedef struct {
    uint32_t **pts;
    uint32_t numpts;
    uint32_t *map;
    uint32_t total_range;
    uint32_t addr_start;
    uint32_t flags;
    uint8_t state;
} drv_pagealloc_data_t;

void drv_pagealloc_init(drv_mem_t *drv);

void pt_set_entry(void *pt, uint16_t n, uint32_t page, uint8_t user, uint8_t rw, uint8_t present);
uint32_t get_physaddr(uint32_t virtualaddr);