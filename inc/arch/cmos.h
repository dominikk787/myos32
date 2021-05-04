#pragma once
#include <stdint.h>

uint8_t get_cmos(uint8_t addr);

extern uint32_t epoch, fattime;

// void print_rtc(void);
void rtc_init(void);