#pragma once

#include <stdint.h>

extern uint32_t pd[1024];

void pd_set_entry(void *pd, uint16_t n, void *pt, uint8_t user, uint8_t rw, uint8_t present);
void pt_set_entry(void *pt, uint16_t n, uint32_t page, uint8_t user, uint8_t rw, uint8_t present);

void alloc_free_pages(void *addr, uint32_t num);
void *alloc_pages(void *addr, uint32_t num, uint8_t user);
void alloc_init();