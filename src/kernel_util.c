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

void *kmalloc(uint32_t size) {
}
void kfree(void* addr) {
}