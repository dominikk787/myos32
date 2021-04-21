#include "kernel.h"
#include "io.h"
#include "intr.h"
#include "multiboot.h"
#include "driver_inout.h"
#include "disk.h"
#include "alloc.h"

#define MEM_ALLOC_SIZE (256 * 1024 * 1024)

extern void cpuid0();
extern void cpuid1();
extern void cpuid3();
extern char cpuid_vendor[12];
extern uint32_t cpuid_max;
extern uint32_t cpuid_version;
extern uint32_t cpuid_additional;
extern uint32_t cpuid_feature[2];
extern uint32_t cpuid_id[2];

uint32_t mb_magic, mb_addr, irq0_print = 0;
volatile uint32_t ms_counter = 0;
uint8_t mbr[512];

drv_screen_data_t screen0_data = {.cursor_start = 11, .cursor_end = 13};
drv_out_t screen0 = {.drv_data = &screen0_data, .user_data = 0};
uint16_t screen0_buf[80*25];

drv_kbd_data_t kbd_data;
drv_in_t kbd = {.drv_data = &kbd_data, .user_data = 0};

uint32_t __attribute__((aligned(4096))) alloc_pts[1024 * 2];
uint32_t alloc_map[MEM_ALLOC_SIZE >> 17];
drv_pagealloc_data_t pagealloc_data = {.pts = alloc_pts, .numpts = 2, .map = alloc_map, .total_range = MEM_ALLOC_SIZE, 
                                        .addr_start = 0xE0000000, .flags = 0};
drv_mem_t pagealloc = {.drv_data = &pagealloc_data, .user_data = 0};

void printf (const char *format, ...);

void do_irq0(void) { 
    static uint16_t i = 0;
    ms_counter++;
    i++;
    if(i >= 1000) {
        if(irq0_print) screen0.ch(&screen0, 'a');
        i = 0;
    }
}

void pit_set_div(uint16_t div) { 
    //unsigned long val=1193180/hz; 
    outportb(0x43, 0x36); 
    outportb(0x40, div & 0xff); 
    outportb(0x40, (div>>8) & 0xff); 
}

void check_multiboot() {
    struct multiboot_tag *tag;
    uint32_t size;

    if (mb_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        printf ("Invalid magic number: 0x%u\n", mb_magic);
        return;
    } else printf("Magic ok 0x%x\n", mb_magic);

    if (mb_addr & 7) {
        printf ("Unaligned mbi: 0x%x\n", mb_addr);
        return;
    }

    size = *(uint32_t *) mb_addr;
    printf ("Announced mbi size 0x%x\n", size);
    for (tag = (struct multiboot_tag *) (mb_addr + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
        tag = (struct multiboot_tag *) ((uint8_t *) tag + ((tag->size + 7) & ~7))) {
        printf ("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);
        switch (tag->type) {
        case MULTIBOOT_TAG_TYPE_CMDLINE:
            printf ("Command line = %s\n",
                ((struct multiboot_tag_string *) tag)->string);
            break;
        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
            printf ("Boot loader name = %s\n",
                ((struct multiboot_tag_string *) tag)->string);
            break;
        case MULTIBOOT_TAG_TYPE_MODULE:
            printf ("Module at 0x%x-0x%x. Command line %s\n",
                ((struct multiboot_tag_module *) tag)->mod_start,
                ((struct multiboot_tag_module *) tag)->mod_end,
                ((struct multiboot_tag_module *) tag)->cmdline);
            break;
        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
            printf ("mem_lower = %uKB, mem_upper = %uKB\n",
                ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
                ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
            break;
        case MULTIBOOT_TAG_TYPE_BOOTDEV:
            printf ("Boot device 0x%x,%u,%u\n",
                ((struct multiboot_tag_bootdev *) tag)->biosdev,
                ((struct multiboot_tag_bootdev *) tag)->slice,
                ((struct multiboot_tag_bootdev *) tag)->part);
            break;
        case MULTIBOOT_TAG_TYPE_MMAP: {
            multiboot_memory_map_t *mmap;

            printf ("mmap\n");

            for (mmap = ((struct multiboot_tag_mmap *) tag)->entries; (uint8_t *) mmap  < (uint8_t *) tag + tag->size;
            mmap = (multiboot_memory_map_t *) ((unsigned long) mmap + ((struct multiboot_tag_mmap *) tag)->entry_size))
                printf (" base_addr = 0x%x%x, length = 0x%x%x, type = 0x%x\n",
                    (uint32_t) (mmap->addr >> 32),
                    (uint32_t) (mmap->addr & 0xffffffff),
                    (uint32_t) (mmap->len >> 32),
                    (uint32_t) (mmap->len & 0xffffffff),
                    (uint32_t) mmap->type);
            }
            break;
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
            uint32_t color;
            unsigned i;
            struct multiboot_tag_framebuffer *tagfb = (struct multiboot_tag_framebuffer *) tag;
            void *fb = (void *) (unsigned long) tagfb->common.framebuffer_addr;

            switch (tagfb->common.framebuffer_type) {
            case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED: {
                uint32_t best_distance, distance;
                struct multiboot_color *palette;

                palette = tagfb->framebuffer_palette;

                color = 0;
                best_distance = 4*256*256;

                for (i = 0; i < tagfb->framebuffer_palette_num_colors; i++) {
                    distance = (0xff - palette[i].blue) * (0xff - palette[i].blue)
                    + palette[i].red * palette[i].red + palette[i].green * palette[i].green;
                    if (distance < best_distance) {
                        color = i;
                        best_distance = distance;
                    }
                }
                }
                break;

            case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
                color = ((1 << tagfb->framebuffer_blue_mask_size) - 1) << tagfb->framebuffer_blue_field_position;
                break;

            case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
                color = '\\' | 0x0100;
                break;

            default:
                color = 0xffffffff;
                break;
            }

            /*for (i = 0; i < tagfb->common.framebuffer_width && i < tagfb->common.framebuffer_height; i++) {
                switch (tagfb->common.framebuffer_bpp) {
                    case 8: {
                        uint8_t *pixel = fb + tagfb->common.framebuffer_pitch * i + i;
                        *pixel = color;
                    }
                    break;
                    case 15:
                    case 16: {
                        uint16_t *pixel = fb + tagfb->common.framebuffer_pitch * i + 2 * i;
                        *pixel = color;
                    }
                    break;
                    case 24: {
                        uint32_t *pixel = fb + tagfb->common.framebuffer_pitch * i + 3 * i;
                        *pixel = (color & 0xffffff) | (*pixel & 0xff000000);
                    }
                    break;

                    case 32: {
                        uint32_t *pixel = fb + tagfb->common.framebuffer_pitch * i + 4 * i;
                        *pixel = color;
                    }
                    break;
                }
            }*/
            break;
            }
        }
    }
    tag = (struct multiboot_tag *) ((uint8_t *) tag + ((tag->size + 7) & ~7));
    printf ("Total mbi size 0x%x\n", (unsigned) tag - mb_addr);
}

void keybord_in(struct _driver_in_t *data, uint8_t ch, uint16_t flags) {
    if((flags & (IN_SPECIAL_ALT | IN_SPECIAL_CTRL)) == 0 && ch >= ' ' && ch < IN_KEY_F1) {
        if(ch == 'M') check_multiboot();
        else if(ch == 'C') irq0_print = 1;
        else if(ch == 'c') irq0_print = 0;
        else if(ch == '0') {
            cpuid0();
            printf("max cpuid: 0x%x\n", cpuid_max);
            screen0.str(&screen0, "cpu vendor: \"");
            for(uint8_t i = 0; i < 12; i++) screen0.ch(&screen0, cpuid_vendor[i]);
            screen0.str(&screen0, "\"\n");
        } else if(ch == '1') {
            cpuid1();
            printf("cpu version: 0x%x\n", cpuid_version);
            printf("cpu additional: 0x%x\n", cpuid_additional);
            printf("cpu features: 0x%x 0x%x\n", cpuid_feature[0], cpuid_feature[1]);
        } else if(ch == '3') {
            cpuid3();
            printf("cpu id: 0x%x 0x%x\n", cpuid_id[0], cpuid_id[1]);
        } else if(ch == 'D') {
            screen0.set_enabled(&screen0, 0, &screen0_buf);
            for(uint32_t i = 0; i < 80*25; i++) {
                ((uint16_t*)0xC00B8000)[i] = ((i / 80) & 0x7) << 12 | (i & 1 ? 1 << 15 : (i < 256 ? i : 0)) | OUT_COLOR_WHITE << 8;
            }
        } else if(ch == 'E') screen0.set_enabled(&screen0, 1, (void*)0xFFFFFFFF);
        else if(ch == 'a') {
            if(pagealloc.alloc(&pagealloc, (void*)0xE0000000, 1) == (void*)0xE0000000) printf("alloc ok\n");
            else printf("alloc error\n");
        } else if(ch == 't') {
            for(uint32_t i = 0; i < (pagealloc.unit / 4); i++) 
                if(i % 256 == 0) printf("%08X ", ((uint32_t*)0xE0000000)[i]);
            printf("\n");
            for(uint32_t i = 0; i < (pagealloc.unit / 4); i++) 
                ((uint32_t*)0xE0000000)[i] ^= 0x5A5A5A5A;
            for(uint32_t i = 0; i < (pagealloc.unit / 4); i++) 
                if(i % 256 == 0) printf("%08X ", ((uint32_t*)0xE0000000)[i]);
            printf("\n");
        } else if(ch == 'f') {
            pagealloc.free(&pagealloc, (void*)0xE0000000, 1);
            printf("free ok\n");
        } else if(ch == 'A') {
            if(pagealloc.alloc(&pagealloc, (void*)0xE0001000, 8) == (void*)0xE0001000) printf("alloc ok\n");
            else printf("alloc error\n");
        } else if(ch == 'T') {
            for(uint32_t i = 0; i < (pagealloc.unit * 2); i++) 
                if(i % 1024 == 0) printf("%08X ", ((uint32_t*)0xE0001000)[i]);
            printf("\n");
            for(uint32_t i = 0; i < (pagealloc.unit * 2); i++) 
                ((uint32_t*)0xE0001000)[i] ^= 0x5A5A5A5A;
            for(uint32_t i = 0; i < (pagealloc.unit * 2); i++) 
                if(i % 1024 == 0) printf("%08X ", ((uint32_t*)0xE0001000)[i]);
            printf("\n");
        } else if(ch == 'F') {
            pagealloc.free(&pagealloc, (void*)0xE0001000, 8);
            printf("free ok\n");
        }
        else screen0.ch(&screen0, ch);
    } else {
        if(flags & IN_SPECIAL_ALT) screen0.str(&screen0, "Alt");
        if(flags & IN_SPECIAL_CTRL) screen0.str(&screen0, "Ctrl");
        if(flags & IN_SPECIAL_SHIFT) screen0.str(&screen0, "Shift");
        if(ch >= ' ' && ch < IN_KEY_F1) screen0.ch(&screen0, ch);
        else {
            if(flags == 0) {
                if(ch == IN_KEY_F1) {
                    screen0.clear(&screen0);
                    for(uint16_t i = 0; i < 256; i++) {
                        printf("%02X ", (uint32_t)mbr[i]);
                        if((i % 16) == 15) printf("\n");
                    }
                } else if(ch == IN_KEY_F2) {
                    screen0.clear(&screen0);
                    for(uint16_t i = 256; i < 512; i++) {
                        printf("%02X ", (uint32_t)mbr[i]);
                        if((i % 16) == 15) printf("\n");
                    }
                } else if(ch == IN_KEY_F3) {
                    screen0.clear(&screen0);
                    printf("status 0x%02X\n", (uint32_t)mbr[446]);
                    printf("partition type 0x%02X\n", (uint32_t)mbr[446 + 4]);
                    printf("first lba 0x%08X\n", ((uint32_t*)(mbr + 446))[2]);
                    printf("last lba 0x%08X\n", ((uint32_t*)(mbr + 446))[3]);
                } else printf("x%02x", (uint32_t)ch);
            } else printf("x%02x", (uint32_t)ch);
        }
    }
}

void start_kernel(uint32_t magic, uint32_t addr) {
    set_trap_gate(0, &exc0);
    set_trap_gate(1, &exc1);
    set_trap_gate(2, &exc2);
    set_trap_gate(3, &exc3);
    set_trap_gate(4, &exc4);
    set_trap_gate(5, &exc5);
    set_trap_gate(6, &exc6);
    set_trap_gate(7, &exc7);
    set_trap_gate(8, &exc8);
    set_trap_gate(9, &exc9);
    set_trap_gate(10, &exc10);
    set_trap_gate(11, &exc11);
    set_trap_gate(12, &exc12);
    set_trap_gate(13, &exc13);
    set_trap_gate(14, &exc14);
    mb_magic = magic;
    mb_addr = addr;
    drv_screen_init(&screen0);
    screen0.set_enabled(&screen0, 2, (void*)0xFFFFFFFF);
    screen0.set_color(&screen0, OUT_COLOR_PINK, OUT_COLOR_BLACK);
    screen0.clear(&screen0);
    printf("Hello World !!!\n");
    kbd.in_clb = keybord_in;
    drv_kbd_init(&kbd);
    pit_set_div(1192); //1000Hz
    set_intr_gate(0x20, &irq0);
    enable_irq(0);
    __asm__ volatile("sti");
    ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
    ide_ata_access(0, 0, 0, 1, 0x10, mbr);
    drv_pagealloc_init(&pagealloc);
    pagealloc.set_state(&pagealloc, 1);
    // uint8_t i = 'a';
    while(1) {
        // screen0.ch(&screen0, i);
        // i++;
        // if(i > 'z') i = 'a';
    }
}

/*  Convert the integer D to a string and save the string in BUF. If
   BASE is equal to ’d’, interpret that D is decimal, and if BASE is
   equal to ’x’, interpret that D is hexadecimal. */
void itoa (char *buf, int base, int d) {
    char *p = buf;
    char *p1, *p2;
    unsigned long ud = d;
    int divisor = 10;
    char a = 'a';

    /*  If %d is specified and D is minus, put ‘-’ in the head. */
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

    /*  Divide UD by DIVISOR until UD == 0. */
    do {
        int remainder = ud % divisor;

        *p++ = (remainder < 10) ? remainder + '0' : remainder + a - 10;
    } while (ud /= divisor);

    /*  Terminate BUF. */
    *p = 0;

    /*  Reverse BUF. */
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

/*  Format a string and print it on the screen, just like the libc
   function printf. */
void printf (const char *format, ...) {
    uint32_t *arg = (uint32_t *) &format;
    int c, argc = 1;
    char buf[20];

    while ((c = *format++) != 0) {
        if (c != '%')
            screen0.ch(&screen0, c);
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
                        screen0.ch(&screen0, pad0 ? '0' : ' ');
                    while (*p)
                        screen0.ch(&screen0, *p++);
                    break;

                default:
                    screen0.ch(&screen0, (char)arg[argc++]);
                    break;
            }
        }
    }
}