bits 32

global cpuid0, cpuid1, cpuid3
global cpuid_vendor, cpuid_max, cpuid_version, cpuid_additional, cpuid_feature, cpuid_id

section .text
cpuid0:
    pushad
    mov eax, 0
    cpuid
    mov [cpuid_max], eax
    mov [cpuid_vendor], ebx
    mov [cpuid_vendor + 4], edx
    mov [cpuid_vendor + 8], ecx
    popad

cpuid1:
    pushad
    mov eax, 1
    cpuid
    mov [cpuid_version], eax
    mov [cpuid_additional], ebx
    mov [cpuid_feature], ecx
    mov [cpuid_feature + 4], edx
    popad

cpuid3:
    pushad
    mov eax, 3
    cpuid
    mov [cpuid_id], eax
    mov [cpuid_id], ebx
    popad

section .bss
cpuid_vendor: resb 12
cpuid_max: resb 4
cpuid_version: resb 4
cpuid_additional: resb 4
cpuid_feature: resb 8
cpuid_id: resb 8