#pragma once

#include <stdint.h>

#define KERNEL_REMAP_ADD 0xC0000000
#define KERNEL_TO_PHYS(addr) ((addr) - KERNEL_REMAP_ADD)

extern volatile uint32_t ms_counter;

void printf (const char *format, ...);