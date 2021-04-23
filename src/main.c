#include "kernel.h"
#include "arch/io.h"
#include "arch/intr.h"
#include "multiboot.h"
#include "disk.h"
#include "drivers.h"
#include "arch/cpuid.h"
#include "ext2.h"

#define MEM_ALLOC_SIZE (256 * 1024 * 1024)

uint32_t mb_magic, mb_addr, irq0_print = 0;
volatile uint32_t ms_counter = 0;
uint8_t mbr[512], disk_data[512];

extern drv_inout_t inout0;

drv_screen_data_t screen0_data = {.cursor_start = 11, .cursor_end = 13};
drv_out_t screen0 = {.drv_data = &screen0_data, .user_data = 0, .inout = &inout0};
uint16_t screen0_buf[80*25];

drv_kbd_data_t kbd_data;
drv_in_t kbd = {.drv_data = &kbd_data, .user_data = 0, .inout = &inout0};

drv_inout_t inout0 = {.in = &kbd, .out = &screen0, .user = (void*)0}, *inout_kernel = &inout0;

uint32_t __attribute__((aligned(4096))) alloc_pts[1024 * 2];
uint32_t alloc_map[MEM_ALLOC_SIZE >> 17];
drv_pagealloc_data_t pagealloc_data = {.pts = alloc_pts, .numpts = 2, .map = alloc_map, .total_range = MEM_ALLOC_SIZE, 
                                        .addr_start = 0xE0000000, .flags = 0};
drv_mem_t pagealloc = {.drv_data = &pagealloc_data, .user_data = 0}, *pagealloc_kernel = &pagealloc;

void do_irq0(void) { 
    static uint16_t i = 0;
    ms_counter++;
    i++;
    if(i >= 1000) {
        if(irq0_print) screen0.ch(&screen0, 'a');
        i = 0;
    }
}

void check_multiboot() {
    struct multiboot_tag *tag;
    uint32_t size;

    if (mb_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        kprint ("Invalid magic number: 0x%u\n", mb_magic);
        return;
    } else kprint("Magic ok 0x%x\n", mb_magic);

    if (mb_addr & 7) {
        kprint ("Unaligned mbi: 0x%x\n", mb_addr);
        return;
    }

    size = *(uint32_t *) mb_addr;
    kprint ("Announced mbi size 0x%x\n", size);
    for (tag = (struct multiboot_tag *) (mb_addr + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
        tag = (struct multiboot_tag *) ((uint8_t *) tag + ((tag->size + 7) & ~7))) {
        kprint ("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);
        switch (tag->type) {
        case MULTIBOOT_TAG_TYPE_CMDLINE:
            kprint ("Command line = %s\n",
                ((struct multiboot_tag_string *) tag)->string);
            break;
        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
            kprint ("Boot loader name = %s\n",
                ((struct multiboot_tag_string *) tag)->string);
            break;
        case MULTIBOOT_TAG_TYPE_MODULE:
            kprint ("Module at 0x%x-0x%x. Command line %s\n",
                ((struct multiboot_tag_module *) tag)->mod_start,
                ((struct multiboot_tag_module *) tag)->mod_end,
                ((struct multiboot_tag_module *) tag)->cmdline);
            break;
        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
            kprint ("mem_lower = %uKB, mem_upper = %uKB\n",
                ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
                ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
            break;
        case MULTIBOOT_TAG_TYPE_BOOTDEV:
            kprint ("Boot device 0x%x,%u,%u\n",
                ((struct multiboot_tag_bootdev *) tag)->biosdev,
                ((struct multiboot_tag_bootdev *) tag)->slice,
                ((struct multiboot_tag_bootdev *) tag)->part);
            break;
        case MULTIBOOT_TAG_TYPE_MMAP: {
            multiboot_memory_map_t *mmap;

            kprint ("mmap\n");

            for (mmap = ((struct multiboot_tag_mmap *) tag)->entries; (uint8_t *) mmap  < (uint8_t *) tag + tag->size;
                    mmap = (multiboot_memory_map_t *) ((unsigned long) mmap + ((struct multiboot_tag_mmap *) tag)->entry_size))
                kprint (" base_addr = 0x%x%x, length = 0x%x%x, type = 0x%x\n",
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
    kprint ("Total mbi size 0x%x\n", (unsigned) tag - mb_addr);
}

void keybord_in(struct _driver_in_t *data, uint8_t ch, uint16_t flags) {
    if((flags & (IN_SPECIAL_ALT | IN_SPECIAL_CTRL)) == 0 && ch >= ' ' && ch < IN_KEY_F1) {
        if(ch == 'M') check_multiboot();
        else if(ch == 'C') irq0_print = 1;
        else if(ch == 'c') irq0_print = 0;
        else if(ch == 'q') {
            cpuid0();
            kprint("max cpuid: 0x%x\n", cpuid_max);
            screen0.str(&screen0, "cpu vendor: \"");
            for(uint8_t i = 0; i < 12; i++) screen0.ch(&screen0, cpuid_vendor[i]);
            screen0.str(&screen0, "\"\n");
        } else if(ch == 'w') {
            cpuid1();
            kprint("cpu version: 0x%x\n", cpuid_version);
            kprint("cpu additional: 0x%x\n", cpuid_additional);
            kprint("cpu features: 0x%x 0x%x\n", cpuid_feature[0], cpuid_feature[1]);
        } else if(ch == 'e') {
            cpuid3();
            kprint("cpu id: 0x%x 0x%x\n", cpuid_id[0], cpuid_id[1]);
        } else if(ch == 'D') {
            screen0.set_enabled(&screen0, 0, &screen0_buf);
            for(uint32_t i = 0; i < 80*25; i++) {
                ((uint16_t*)0xC00B8000)[i] = ((i / 80) & 0x7) << 12 | (i & 1 ? 1 << 15 : (i < 256 ? i : 0)) | OUT_COLOR_WHITE << 8;
            }
        } else if(ch == 'E') screen0.set_enabled(&screen0, 1, (void*)0xFFFFFFFF);
        else if(ch == 'a') {
            if(pagealloc.alloc(&pagealloc, (void*)0xE0000000, 1) == (void*)0xE0000000) kprint("alloc ok\n");
            else kprint("alloc error\n");
        } else if(ch == 't') {
            for(uint32_t i = 0; i < (pagealloc.unit / 4); i++) 
                if(i % 256 == 0) kprint("%08X ", ((uint32_t*)0xE0000000)[i]);
            kprint("\n");
            for(uint32_t i = 0; i < (pagealloc.unit / 4); i++) 
                ((uint32_t*)0xE0000000)[i] ^= 0x5A5A5A5A;
            for(uint32_t i = 0; i < (pagealloc.unit / 4); i++) 
                if(i % 256 == 0) kprint("%08X ", ((uint32_t*)0xE0000000)[i]);
            kprint("\n");
        } else if(ch == 'f') {
            pagealloc.free(&pagealloc, (void*)0xE0000000, 1);
            kprint("free ok\n");
        } else if(ch == 'A') {
            if(pagealloc.alloc(&pagealloc, (void*)0xE0001000, 8) == (void*)0xE0001000) kprint("alloc ok\n");
            else kprint("alloc error\n");
        } else if(ch == 'T') {
            for(uint32_t i = 0; i < (pagealloc.unit * 2); i++) 
                if(i % 1024 == 0) kprint("%08X ", ((uint32_t*)0xE0001000)[i]);
            kprint("\n");
            for(uint32_t i = 0; i < (pagealloc.unit * 2); i++) 
                ((uint32_t*)0xE0001000)[i] ^= 0x5A5A5A5A;
            for(uint32_t i = 0; i < (pagealloc.unit * 2); i++) 
                if(i % 1024 == 0) kprint("%08X ", ((uint32_t*)0xE0001000)[i]);
            kprint("\n");
        } else if(ch == 'F') {
            pagealloc.free(&pagealloc, (void*)0xE0001000, 8);
            kprint("free ok\n");
        } else if(ch >= '0' && ch <= '9') {
            uint32_t n = ch - '0' + ((uint32_t*)(mbr + 446))[2];
            kprint("reading lba %u\n", n);
            ide_ata_access(0, 0, n, 1, 0x10, disk_data);
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
                        kprint("%02X ", (uint32_t)mbr[i]);
                        if((i % 16) == 15) kprint("\n");
                    }
                } else if(ch == IN_KEY_F2) {
                    screen0.clear(&screen0);
                    for(uint16_t i = 256; i < 512; i++) {
                        kprint("%02X ", (uint32_t)mbr[i]);
                        if((i % 16) == 15) kprint("\n");
                    }
                } else if(ch == IN_KEY_F3) {
                    screen0.clear(&screen0);
                    kprint("status 0x%02X\n", (uint32_t)mbr[446]);
                    kprint("partition type 0x%02X\n", (uint32_t)mbr[446 + 4]);
                    kprint("first lba 0x%08X\n", ((uint32_t*)(mbr + 446))[2]);
                    kprint("last lba 0x%08X\n", ((uint32_t*)(mbr + 446))[3]);
                } else if(ch == IN_KEY_F10) {
                    screen0.clear(&screen0);
                    for(uint16_t i = 0; i < 256; i++) {
                        kprint("%02X ", (uint32_t)disk_data[i]);
                        if((i % 16) == 15) kprint("\n");
                    }
                } else if(ch == IN_KEY_F11) {
                    screen0.clear(&screen0);
                    for(uint16_t i = 256; i < 512; i++) {
                        kprint("%02X ", (uint32_t)disk_data[i]);
                        if((i % 16) == 15) kprint("\n");
                    }
                } else if(ch == IN_KEY_F5) {
                    screen0.clear(&screen0);
                    ext2_print_sb();
                } else if(ch == IN_KEY_F6) {
                    screen0.clear(&screen0);
                    ext2_print_bgdt();
                } else if(ch == IN_KEY_F7) {
                    screen0.clear(&screen0);
                    ext2_print_inode();
                } else kprint("x%02x", (uint32_t)ch);
            } else kprint("x%02x", (uint32_t)ch);
        }
    }
}

static void init_int() {
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
    set_intr_gate(0x27, &irq7);
    set_intr_gate(0x2F, &irq15);
}
static void init_driver() {
    drv_screen_init(&screen0);
    screen0.set_enabled(&screen0, 2, (void*)0xFFFFFFFF);
    screen0.set_color(&screen0, OUT_COLOR_PINK, OUT_COLOR_BLACK);
    screen0.clear(&screen0);

    kbd.in_clb = keybord_in;
    drv_kbd_init(&kbd);

    drv_pagealloc_init(&pagealloc);
    pagealloc.set_state(&pagealloc, 1);
}
static void init_pit() { 
    //unsigned long val=1193180/hz; 1192 -> 1000Hz
    outportb(0x43, 0x36); 
    outportb(0x40, 1192 & 0xff); 
    outportb(0x40, (1192>>8) & 0xff); 
    set_intr_gate(0x20, &irq0);
    enable_irq(0);
}

void start_kernel(uint32_t magic, uint32_t addr) {
    mb_magic = magic;
    mb_addr = addr;
    init_int();
    init_driver();
    init_pit();
    kmalloc_init(0xE0100000, 0);
    kprint("Hello World !!!\n");
    asm("sti");
    ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
    ide_ata_access(0, 0, 0, 1, 0x10, mbr);
    ext2_init(((uint32_t*)(mbr + 446))[2]);
    while(1) ;
}