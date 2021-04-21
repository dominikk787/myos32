#pragma once

#include <stdint.h>

void cpuid0();
void cpuid1();
void cpuid3();
extern char cpuid_vendor[12];
extern uint32_t cpuid_max;
extern uint32_t cpuid_version;
extern uint32_t cpuid_additional;
extern uint32_t cpuid_feature[2];
extern uint32_t cpuid_id[2];