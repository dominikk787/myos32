#pragma once

#include "drivers.h"

#include <stdint.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))

extern volatile uint32_t ms_counter;
extern drv_inout_t *inout_kernel;
extern drv_mem_t *pagealloc_kernel;

void kprint(const char *format, ...);

void kmalloc_init(uint32_t start, uint32_t current_size);
void *kmalloc(uint32_t size);
void kfree(void *addr);