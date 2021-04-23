#include "kernel.h"

//  Convert the integer D to a string and save the string in BUF. If
//  BASE is equal to ’d’, interpret that D is decimal, and if BASE is
//  equal to ’x’, interpret that D is hexadecimal.
static void itoa (char *buf, int base, int d) {
    char *p = buf;
    char *p1, *p2;
    unsigned long ud = d;
    int divisor = 10;
    char a = 'a';

    //  If %d is specified and D is minus, put ‘-’ in the head.
    if (base == 'd' && d < 0) {
        *p++ = '-';
        buf++;
        ud = -d;
    }
    else if (base == 'x') divisor = 16;
    else if(base == 'X') {
        divisor = 16;
        a = 'A';
    }

    // Divide UD by DIVISOR until UD == 0.
    do {
        int remainder = ud % divisor;

        *p++ = (remainder < 10) ? remainder + '0' : remainder + a - 10;
    } while (ud /= divisor);

    //  Terminate BUF.
    *p = 0;

    //  Reverse BUF. 
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

//  Format a string and print it on the screen, just like the libc
//   function kprint.
void kprint (const char *format, ...) {
    uint32_t *arg = (uint32_t *) &format;
    int c, argc = 1;
    char buf[20];

    while ((c = *format++) != 0) {
        if (c != '%')
            inout_kernel->out->ch(inout_kernel->out, c);
        else {
            char *p, *p2;
            int pad0 = 0, pad = 0;

            c = *format++;
            if (c == '0') {
                pad0 = 1;
                c = *format++;
            }

            if (c >= '0' && c <= '9') {
                pad = c - '0';
                c = *format++;
            }

            switch (c)
            {
                case 'd':
                case 'u':
                case 'x':
                case 'X':
                    itoa (buf, c, (int)arg[argc++]);
                    p = buf;
                    goto string;
                    break;

                case 's':
                    p = (char*)arg[argc++];
                    if (! p)
                        p = "(null)";

                string:
                    for (p2 = p; *p2; p2++);
                    for (; p2 < p + pad; p2++)
                        inout_kernel->out->ch(inout_kernel->out, pad0 ? '0' : ' ');
                    while (*p)
                        inout_kernel->out->ch(inout_kernel->out, *p++);
                    break;

                default:
                    inout_kernel->out->ch(inout_kernel->out, (char)arg[argc++]);
                    break;
            }
        }
    }
}

typedef struct {
    uint32_t used:1;
    uint32_t size:31;
} kmalloc_entry;

static uint32_t mem_size = 0;
static void *mem_start = 0;
void kmalloc_init(uint32_t start, uint32_t current_size) {
    mem_start = (void*)start;
    mem_size = current_size;
}
void *kmalloc(uint32_t size) {
    uint8_t found = 0;
    uint32_t at = 0;
    for(uint32_t a = 0; a < mem_size;) {
        kmalloc_entry *entry = mem_start + a;
        if(entry->used) {
            kprint("found used %u\n", a);
            a += entry->size + sizeof(kmalloc_entry);
            continue;
        }
        if((a + entry->size + sizeof(kmalloc_entry)) < mem_size) {
            kmalloc_entry *next = mem_start + a + entry->size + sizeof(kmalloc_entry);
            if(next->used == 0) {
                kprint("found next free %u %u\n", a, entry->size + next->size + sizeof(kmalloc_entry));
                entry->size += next->size + sizeof(kmalloc_entry);
                continue;
            }
        } else if(entry->used == 0) {
            at = a;
            found = 2;
            break;
        } else {
            at = a + entry->size + sizeof(kmalloc_entry);
            found = 0;
        }
        if(entry->size >= size) {
            kprint("found free %u\n", a);
            found = 1;
            at = a;
            break;
        }
        a += entry->size + sizeof(kmalloc_entry);
    }
    kprint("kmalloc %u at %u have %u %08X\n", found, at, mem_size, mem_start);
    if(found != 1) {
        uint32_t rest_size = size;
        if(found == 2) {
            kmalloc_entry *entry = mem_start + at;
            rest_size -= entry->size;
        }
        uint32_t min_alloc = rest_size + (found == 0 ? sizeof(kmalloc_entry) : 0);
        uint32_t num_alloc = (min_alloc / pagealloc_kernel->unit) + ((min_alloc % pagealloc_kernel->unit) == 0 ? 0 : 1);
        kprint("kmalloc %u %u\n", min_alloc, num_alloc);
        if(pagealloc_kernel->alloc(pagealloc_kernel, mem_start + mem_size, num_alloc) != mem_start + mem_size) return (void*)0;
        if(found == 0) ((kmalloc_entry*)mem_start + mem_size)->size = num_alloc * pagealloc_kernel->unit - sizeof(kmalloc_entry);
        else {
            kmalloc_entry *entry = mem_start + at;
            entry->size += num_alloc * pagealloc_kernel->unit;
        }
        mem_size += num_alloc * pagealloc_kernel->unit;
        kprint("kmalloc page %u %08X\n", num_alloc, (uint32_t)mem_start + mem_size);
    }
    kmalloc_entry *entry = mem_start + at;
    if(entry->size < size) return (void*)0;
    else if(entry->size >= size + sizeof(kmalloc_entry)) {
        kmalloc_entry *next = mem_start + at + sizeof(kmalloc_entry) + size;
        next->size = entry->size - size - sizeof(kmalloc_entry);
        next->used = 0;
        entry->size = size;
        entry->used = 1;
    }
    else entry->used = 1;
    return mem_start + at + sizeof(kmalloc_entry);
}
void kfree(void* addr) {
    kprint("kfree %08X\n", (uint32_t)addr);
    kmalloc_entry *entry = addr - sizeof(kmalloc_entry);
    if((void*)entry < mem_start || (void*)entry > (mem_start + mem_size - sizeof(kmalloc_entry))) return;
    entry->used = 0;
}