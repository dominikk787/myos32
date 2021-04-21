#pragma once

#include <stdint.h>
#include "drivers.h"

extern volatile uint32_t ms_counter;
extern drv_inout_t *inout_kernel;
extern drv_mem_t *pagealloc_kernel;

void kprint(const char *format, ...);
void *kmalloc(uint32_t size);
void kfree(void* addr);