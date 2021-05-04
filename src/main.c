#include "arch/cmos.h"
#include "arch/cpuid.h"
#include "arch/intr.h"
#include "arch/io.h"
#include "disk.h"
#include "drivers.h"
#include "fatfs/ff.h"
#include "kernel.h"
#include "multiboot.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t mb_magic, mb_addr, irq0_print = 0;
volatile uint32_t ms_counter = 0;
static uint8_t mbr[512], disk_data[512];
static uint32_t __attribute__((aligned(4096))) pt_vga[1024] = {0};

const uint8_t psf_font_lat2_vga16[] = {
#include "Lat2-VGA16.inc"
};

extern drv_inout_t inout0;

drv_screen_text_data_t screen0t_data = {.cursor_start = 11, .cursor_end = 13};
drv_screen_graphic_data_t screen0g_data
  = {.font = (void *)psf_font_lat2_vga16, .cursor_start = 11, .cursor_end = 13};
drv_out_t screen0 = {.user_data = 0, .inout = &inout0};
uint16_t screen0_buf[80 * 25];

drv_kbd_data_t kbd_data;
drv_in_t kbd = {.drv_data = &kbd_data, .user_data = 0, .inout = &inout0};

drv_inout_t inout0 = {.in = &kbd, .out = &screen0, .user = (void *)0};
drv_inout_t *inout_kernel = (void *)0;

uint32_t __attribute__((aligned(4096))) pagealloc_pts[1024];
uint32_t *pagealloc_tpts = pagealloc_pts;
uint32_t pagealloc_map[4096 / 4 / 32];
drv_pagealloc_data_t pagealloc_data = {.pts = &pagealloc_tpts,
                                       .numpts = 1,
                                       .map = pagealloc_map,
                                       .total_range = 1024,
                                       .addr_start = 0xDFC00000,
                                       .flags = 0};
drv_mem_t pagealloc_drv = {.drv_data = &pagealloc_data, .user_data = 0};

uint32_t __attribute__((aligned(4096))) malloc_pts[2048];
uint32_t *malloc_tpts[2] = {malloc_pts, malloc_pts + 1024};
drv_pagealloc_data_t malloc_data = {.pts = malloc_tpts,
                                    .numpts = 1,
                                    .map = (void *)0xDFC00000,
                                    .total_range = 0,
                                    .addr_start = 0xE0000000,
                                    .flags = 0};
drv_mem_t malloc_drv = {.drv_data = &malloc_data, .user_data = 0};
drv_mem_t *pagealloc_kernel = &malloc_drv;

drv_fs_ext2_data_t ext2_data;
drv_fs_exfat_data_t exfat_data;

static uint8_t multiboot_ok = 0, multiboot_fb_text;
static uint32_t multiboot_fb_width, multiboot_fb_height, multiboot_fb_bpp,
  multiboot_fb_pitch, multiboot_mem_low, multiboot_mem_high;
static char *multiboot_cmd, *multiboot_bootloader;
static void *multiboot_fb;

FATFS fat_data;

void do_irq0(void) {
    static uint16_t i = 0;
    ms_counter++;
    i++;
    if(i >= 1000) {
        if(irq0_print) printf("a");
        i = 0;
    }
}

void test_multiboot(void) {
    struct multiboot_tag *tag;
    uint32_t size;

    if(mb_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        printf("Invalid magic number: 0x%lX\n", mb_magic);
        return;
    } else
        printf("Magic ok 0x%lX\n", mb_magic);

    if(mb_addr & 7) {
        printf("Unaligned mbi: 0x%lx\n", mb_addr);
        return;
    }

    size = *(uint32_t *)mb_addr;
    printf("Announced mbi size 0x%lx\n", size);
    for(tag = (struct multiboot_tag *)(mb_addr + 8);
        tag->type != MULTIBOOT_TAG_TYPE_END;
        tag = (struct multiboot_tag *)((uint8_t *)tag
                                       + ((tag->size + 7) & (uint32_t)~7))) {
        printf("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);
        switch(tag->type) {
        case MULTIBOOT_TAG_TYPE_CMDLINE:
            printf("Command line = %s\n",
                   ((struct multiboot_tag_string *)tag)->string);
            break;
        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
            printf("Boot loader name = %s\n",
                   ((struct multiboot_tag_string *)tag)->string);
            break;
        case MULTIBOOT_TAG_TYPE_MODULE:
            printf("Module at 0x%x-0x%x. Command line %s\n",
                   ((struct multiboot_tag_module *)tag)->mod_start,
                   ((struct multiboot_tag_module *)tag)->mod_end,
                   ((struct multiboot_tag_module *)tag)->cmdline);
            break;
        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
            printf("mem_lower = %uKB, mem_upper = %uKB\n",
                   ((struct multiboot_tag_basic_meminfo *)tag)->mem_lower,
                   ((struct multiboot_tag_basic_meminfo *)tag)->mem_upper);
            break;
        case MULTIBOOT_TAG_TYPE_BOOTDEV:
            printf("Boot device 0x%x,%u,%u\n",
                   ((struct multiboot_tag_bootdev *)tag)->biosdev,
                   ((struct multiboot_tag_bootdev *)tag)->slice,
                   ((struct multiboot_tag_bootdev *)tag)->part);
            break;
        case MULTIBOOT_TAG_TYPE_MMAP: {
            multiboot_memory_map_t *mmap;

            printf("mmap\n");

            for(mmap = ((struct multiboot_tag_mmap *)tag)->entries;
                (uint8_t *)mmap < (uint8_t *)tag + tag->size;
                mmap = (multiboot_memory_map_t
                          *)((unsigned long)mmap
                             + ((struct multiboot_tag_mmap *)tag)->entry_size))
                printf(
                  " base_addr = 0x%lx%lx, length = 0x%lx%lx, type = 0x%lx\n",
                  (uint32_t)(mmap->addr >> 32),
                  (uint32_t)(mmap->addr & 0xffffffff),
                  (uint32_t)(mmap->len >> 32),
                  (uint32_t)(mmap->len & 0xffffffff), (uint32_t)mmap->type);
        } break;
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
            uint32_t color;
            unsigned i;
            struct multiboot_tag_framebuffer *tagfb
              = (struct multiboot_tag_framebuffer *)tag;
            // void *fb = (void *) (unsigned long)
            // tagfb->common.framebuffer_addr;

            switch(tagfb->common.framebuffer_type) {
            case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED: {
                uint32_t best_distance, distance;
                struct multiboot_color *palette;

                palette = tagfb->framebuffer_palette;

                color = 0;
                best_distance = 4 * 256 * 256;

                for(i = 0; i < tagfb->framebuffer_palette_num_colors; i++) {
                    distance = (uint32_t)((0xff - palette[i].blue)
                                          * (0xff - palette[i].blue))
                               + (palette[i].red * palette[i].red)
                               + (palette[i].green * palette[i].green);
                    if(distance < best_distance) {
                        color = i;
                        best_distance = distance;
                    }
                }
            } break;

            case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
                color = (uint32_t)((1 << tagfb->framebuffer_blue_mask_size) - 1)
                        << tagfb->framebuffer_blue_field_position;
                break;

            case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
                color = '\\' | 0x0100;
                break;

            default: color = 0xffffffff; break;
            }
            (void)color;

            /*for (i = 0; i < tagfb->common.framebuffer_width && i <
            tagfb->common.framebuffer_height; i++) { switch
            (tagfb->common.framebuffer_bpp) { case 8: { uint8_t *pixel = fb +
            tagfb->common.framebuffer_pitch * i + i; *pixel = color;
                    }
                    break;
                    case 15:
                    case 16: {
                        uint16_t *pixel = fb + tagfb->common.framebuffer_pitch *
            i + 2
            * i; *pixel = color;
                    }
                    break;
                    case 24: {
                        uint32_t *pixel = fb + tagfb->common.framebuffer_pitch *
            i + 3
            * i; *pixel = (color & 0xffffff) | (*pixel & 0xff000000);
                    }
                    break;

                    case 32: {
                        uint32_t *pixel = fb + tagfb->common.framebuffer_pitch *
            i + 4
            * i; *pixel = color;
                    }
                    break;
                }
            }*/
            break;
        }
        }
    }
    tag = (struct multiboot_tag *)((uint8_t *)tag
                                   + ((tag->size + 7) & (uint32_t)~7));
    printf("Total mbi size 0x%lx\n", (unsigned)tag - mb_addr);
}

void parse_multiboot(void) {
    struct multiboot_tag *tag;

    multiboot_ok = 0;

    if(mb_magic != MULTIBOOT2_BOOTLOADER_MAGIC) { return; }
    if(mb_addr & 7) { return; }
    for(tag = (struct multiboot_tag *)(mb_addr + 8);
        tag->type != MULTIBOOT_TAG_TYPE_END;
        tag = (struct multiboot_tag *)((uint8_t *)tag
                                       + ((tag->size + 7) & (uint32_t)~7))) {
        switch(tag->type) {
        case MULTIBOOT_TAG_TYPE_CMDLINE:
            multiboot_cmd = ((struct multiboot_tag_string *)tag)->string;
            break;
        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
            multiboot_bootloader = ((struct multiboot_tag_string *)tag)->string;
            break;
        // case MULTIBOOT_TAG_TYPE_MODULE:
        //     printf ("Module at 0x%x-0x%x. Command line %s\n",
        //         ((struct multiboot_tag_module *) tag)->mod_start,
        //         ((struct multiboot_tag_module *) tag)->mod_end,
        //         ((struct multiboot_tag_module *) tag)->cmdline);
        //     break;
        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
            multiboot_mem_low
              = ((struct multiboot_tag_basic_meminfo *)tag)->mem_lower;
            multiboot_mem_high
              = ((struct multiboot_tag_basic_meminfo *)tag)->mem_upper;
            break;
        // case MULTIBOOT_TAG_TYPE_BOOTDEV:
        //     printf ("Boot device 0x%x,%u,%u\n",
        //         ((struct multiboot_tag_bootdev *) tag)->biosdev,
        //         ((struct multiboot_tag_bootdev *) tag)->slice,
        //         ((struct multiboot_tag_bootdev *) tag)->part);
        //     break;
        // case MULTIBOOT_TAG_TYPE_MMAP: {
        //     multiboot_memory_map_t *mmap;

        //     printf ("mmap\n");

        //     for (mmap = ((struct multiboot_tag_mmap *) tag)->entries;
        //     (uint8_t *) mmap  < (uint8_t *) tag + tag->size;
        //             mmap = (multiboot_memory_map_t *) ((unsigned long) mmap +
        //             ((struct multiboot_tag_mmap *) tag)->entry_size))
        //         printf (" base_addr = 0x%lx%lx, length = 0x%lx%lx, type =
        //         0x%lx\n",
        //             (uint32_t) (mmap->addr >> 32),
        //             (uint32_t) (mmap->addr & 0xffffffff),
        //             (uint32_t) (mmap->len >> 32),
        //             (uint32_t) (mmap->len & 0xffffffff),
        //             (uint32_t) mmap->type);
        //     }
        //     break;
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
            struct multiboot_tag_framebuffer *tagfb
              = (struct multiboot_tag_framebuffer *)tag;
            multiboot_fb
              = (void *)(unsigned long)tagfb->common.framebuffer_addr;

            switch(tagfb->common.framebuffer_type) {
            case MULTIBOOT_FRAMEBUFFER_TYPE_RGB: multiboot_fb_text = 0; break;
            case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
                multiboot_fb_text = 1;
                break;
            default: multiboot_fb_text = 0xFF; break;
            }
            multiboot_fb_width = tagfb->common.framebuffer_width;
            multiboot_fb_height = tagfb->common.framebuffer_height;
            multiboot_fb_bpp = tagfb->common.framebuffer_bpp;
            multiboot_fb_pitch = tagfb->common.framebuffer_pitch;
            break;
        }
        }
    }

    multiboot_ok = 1;
}

void keybord_in(drv_in_t *drv, uint8_t ch, uint16_t flags) {
    (void)drv;
    if((flags & (IN_SPECIAL_ALT | IN_SPECIAL_CTRL)) == 0 && ch >= ' '
       && ch < IN_KEY_F1) {
        if(ch == 'M') test_multiboot();
        else if(ch == 'C')
            irq0_print = 1;
        else if(ch == 'c')
            irq0_print = 0;
        else if(ch == 'q') {
            cpuid0();
            printf("max cpuid: 0x%lx\n", cpuid_max);
            printf("cpu vendor: \"");
            for(uint8_t i = 0; i < 12; i++) printf("%c", cpuid_vendor[i]);
            printf("\"\n");
        } else if(ch == 'w') {
            cpuid1();
            printf("cpu version: 0x%lx\n", cpuid_version);
            printf("cpu additional: 0x%lx\n", cpuid_additional);
            printf("cpu features: 0x%lx 0x%lx\n", cpuid_feature[0],
                   cpuid_feature[1]);
        } else if(ch == 'e') {
            cpuid3();
            printf("cpu id: 0x%lx 0x%lx\n", cpuid_id[0], cpuid_id[1]);
        } else if(ch == 'D') {
            screen0.set_enabled(&screen0, 0, &screen0_buf);
            for(uint32_t i = 0; i < 80 * 25; i++) {
                ((uint16_t *)0xC00B8000)[i] = (uint16_t)(
                  ((i / 80) & 0x7) << 12 | (i & 1 ? 1 << 15 : (i < 256 ? i : 0))
                  | OUT_COLOR_WHITE << 8);
            }
        } else if(ch == 'E')
            screen0.set_enabled(&screen0, 1, (void *)0xFFFFFFFF);
        else if(ch >= '0' && ch <= '9') {
            uint32_t n = ch - '0' + ((uint32_t *)(mbr + 446))[2];
            printf("reading lba %lu\n", n);
            ide_read_sectors(0, 1, n, disk_data);
        } else if(ch == 'T') {
            float x = 1.0;
            x -= 1.0;
            x = 10 / x;
            printf("%f\n", x);
        } else if(ch == 'X') {
            f_mount(0, "0", 0);
        } else
            printf("%c", ch);
    } else {
        if(flags & IN_SPECIAL_ALT) printf("Alt");
        if(flags & IN_SPECIAL_CTRL) printf("Ctrl");
        if(flags & IN_SPECIAL_SHIFT) printf("Shift");
        if(ch >= ' ' && ch < IN_KEY_F1) printf("%c", ch);
        else {
            if(flags == 0) {
                if(ch == IN_KEY_F1) {
                    screen0.clear(&screen0);
                    for(uint16_t i = 0; i < 256; i++) {
                        printf("%02X ", mbr[i]);
                        if((i % 16) == 15) printf("\n");
                    }
                } else if(ch == IN_KEY_F2) {
                    screen0.clear(&screen0);
                    for(uint16_t i = 256; i < 512; i++) {
                        printf("%02X ", mbr[i]);
                        if((i % 16) == 15) printf("\n");
                    }
                } else if(ch == IN_KEY_F3) {
                    screen0.clear(&screen0);
                    printf("status 0x%02X\n", mbr[446]);
                    printf("partition type 0x%02X\n", mbr[446 + 4]);
                    printf("first lba 0x%08lX\n", ((uint32_t *)(mbr + 446))[2]);
                    printf("last lba 0x%08lX\n", ((uint32_t *)(mbr + 446))[3]);
                } else if(ch == IN_KEY_F10) {
                    screen0.clear(&screen0);
                    for(uint16_t i = 0; i < 256; i++) {
                        printf("%02X ", disk_data[i]);
                        if((i % 16) == 15) printf("\n");
                    }
                } else if(ch == IN_KEY_F11) {
                    screen0.clear(&screen0);
                    for(uint16_t i = 256; i < 512; i++) {
                        printf("%02X ", disk_data[i]);
                        if((i % 16) == 15) printf("\n");
                    }
                } else if(ch == IN_KEY_F5) {
                    screen0.clear(&screen0);
                    ext2_print_sb(&ext2_data);
                } else if(ch == IN_KEY_F6) {
                    screen0.clear(&screen0);
                    ext2_print_bgdt(&ext2_data);
                } else if(ch == IN_KEY_F7) {
                    screen0.clear(&screen0);
                    ext2_print_inodes(&ext2_data);
                } else if(ch == IN_KEY_F8) {
                    screen0.clear(&screen0);
                    // ext2_print_inodes(&ext2_data);
                    // ext2_dump_inode(&ext2_data, ext2_find_inode(&ext2_data,
                    // 2,
                    // "/grub/fonts/unicode.pf2"));
                    ext2_dump_inode(&ext2_data,
                                    ext2_find_inode(&ext2_data, 2, "/test"));
                } else
                    printf("x%02x", ch);
            } else
                printf("x%02x", ch);
        }
    }
}

static void init_int(void) {
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
    set_trap_gate(16, &exc16);
    set_intr_gate(0x27, &irq7);
    set_intr_gate(0x2F, &irq15);
}
static void init_mem(void) {
    drv_pagealloc_init(&pagealloc_drv);
    pagealloc_drv.set_state(&pagealloc_drv, 1);
    uint32_t bitmap_pages = ((multiboot_mem_high / 4) + 31) / 32;
    bitmap_pages = (bitmap_pages + 4095) / 4096;
    pagealloc_drv.alloc(&pagealloc_drv, (void *)0xDFC00000, bitmap_pages);
    memcpy((void *)0xDFC00000, pagealloc_map, 32);
    pagealloc_data.map = (void *)0xDFC00000;
    pagealloc_data.total_range = multiboot_mem_high / 4;
    malloc_data.total_range = multiboot_mem_high / 4;
    drv_pagealloc_init(&malloc_drv);
    malloc_drv.set_state(&malloc_drv, 1);
}
static void init_pit(void) {
    // unsigned long val=1193180/hz; 1192 -> 1000Hz
    outb(0x43, 0x36);
    outb(0x40, 1192 & 0xff);
    outb(0x40, (1192 >> 8) & 0xff);
    set_intr_gate(0x20, &irq0);
    enable_irq(0);
}
static void init_screen(void) {
    uint32_t size = multiboot_fb_pitch * multiboot_fb_height;
    uint32_t pages = (size + 4095) / 4096;
    printf("alloc screen %lu\n", pages);
    if(pages > 1024) {
        printf("screen buffer too big %luKB max 4096KB\n", pages * 4);
        while(1) asm("hlt");
        return;
    }
    pt_set_entry((void *)0xFFFFF000, 0xD00 >> 2, get_physaddr((uint32_t)pt_vga),
                 0, 1, 1);
    for(uint16_t i = 0; i < pages; i++) {
        pt_set_entry(pt_vga, i, (uint32_t)multiboot_fb + (i * 4096), 0, 1, 1);
        invlpg(0xD0000000 + (i * 4096));
    }
    if(multiboot_fb_bpp == 32) {
        float scale
          = sqrtf((float)(multiboot_fb_width * multiboot_fb_width
                          + multiboot_fb_height * multiboot_fb_height));
        for(uint16_t y = 0; y < multiboot_fb_height; y++) {
            uint32_t *line
              = (uint32_t *)(0xD0000000 + (y * multiboot_fb_pitch));
            for(uint16_t x = 0; x < multiboot_fb_width; x++) {
                float r = sqrtf((float)(x * x + y * y)) / scale * 256,
                      g = sqrtf((float)((multiboot_fb_width - 1 - x)
                                          * (multiboot_fb_width - 1 - x)
                                        + y * y)),
                      b = sqrtf((float)(x * x
                                        + (multiboot_fb_height - 1 - y)
                                            * (multiboot_fb_height - 1 - y)));
                uint8_t rb = (uint8_t)min((uint16_t)r, 255),
                        gb = (uint8_t)min((uint16_t)g, 255),
                        bb = (uint8_t)min((uint16_t)b, 255);
                line[x] = (rb << 16) | (gb << 8) | bb | (line[x] & 0xFF000000);
            }
        }
    } else if(multiboot_fb_bpp == 24) {
        for(uint16_t y = 0; y < multiboot_fb_height; y++) {
            uint8_t *line = (uint8_t *)(0xD0000000 + (y * multiboot_fb_pitch));
            for(uint16_t x = 0; x < multiboot_fb_width; x++) {
                line[x * 3] = 128;
                line[x * 3 + 1] = 128;
                line[x * 3 + 2] = 128;
            }
        }
    } else if(multiboot_fb_bpp == 15) {
        for(uint16_t y = 0; y < multiboot_fb_height; y++) {
            uint16_t *line
              = (uint16_t *)(0xD0000000 + (y * multiboot_fb_pitch));
            for(uint16_t x = 0; x < multiboot_fb_width; x++) { line[x] = 0; }
        }
    }

    if(multiboot_fb_text == 1) {
        screen0.drv_data = &screen0t_data;
        drv_screen_text_init(&screen0);
        screen0.set_enabled(&screen0, 2, (void *)0xFFFFFFFF);
        screen0.set_color(&screen0, OUT_COLOR_PINK, OUT_COLOR_BLACK);
        screen0.clear(&screen0);
        inout_kernel = &inout0;
    } else if(multiboot_fb_text == 0) {
        screen0.drv_data = &screen0g_data;
        screen0g_data.width = multiboot_fb_width;
        screen0g_data.height = multiboot_fb_height;
        screen0g_data.pitch = multiboot_fb_pitch;
        screen0g_data.bpp = multiboot_fb_bpp;
        drv_screen_graphic_init(&screen0);
        screen0.set_enabled(&screen0, 2, (void *)0xFFFFFFFF);
        screen0.set_color(&screen0, 0xFFFFFF, 0x000000);
        screen0.clear(&screen0);
        inout_kernel = &inout0;
    }
}
static void init_uart(void) {
    outb(0x3F8 + 1, 0x00);  // Disable all interrupts
    outb(0x3F8 + 3, 0x80);  // Enable DLAB (set baud rate divisor)
    outb(0x3F8 + 0, 0x01);  // Set divisor to 1 (lo byte) 115200 baud
    outb(0x3F8 + 1, 0x00);  //                  (hi byte)
    outb(0x3F8 + 3, 0x03);  // 8 bits, no parity, one stop bit
    // outb(0x3F8 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte
    // threshold
    outb(0x3F8 + 2, 0x00);
    outb(0x3F8 + 1, 0x01);  // Enable data ready interrupt
    outb(0x3F8 + 4, 0x04);  // enable out1
    set_intr_gate(0x24, irq4);
    enable_irq(4);
    write_serial('a');
}
static void init_kbd(void) {
    kbd.in_clb = keybord_in;
    drv_kbd_init(&kbd);
}

void do_irq4(void) { printf("uart in %02X\n", inb(0x3F8)); }

FRESULT scan_files(char *path) {
    FRESULT res;
    DIR dir;
    UINT i;
    static FILINFO fno;

    res = f_opendir(&dir, path);  // Open the directory
    if(res == FR_OK) {
        for(;;) {
            res = f_readdir(&dir, &fno);  // Read a directory item
            if(res != FR_OK || fno.fname[0] == 0)
                break;                  // Break on error or end of dir
            if(fno.fattrib & AM_DIR) {  // It is a directory
                i = strlen(path);
                sprintf(&path[i], "/%s", fno.fname);
                res = scan_files(path);  // Enter the directory
                if(res != FR_OK) break;
                path[i] = 0;
            } else {  // It is a file.
                printf("%s/%s\n", path, fno.fname);
            }
        }
        f_closedir(&dir);
    } else
        printf("opendir err %u\n", res);

    return res;
}

void start_kernel(uint32_t magic, uint32_t addr) {
    mb_magic = magic;
    mb_addr = addr;
    init_int();
    init_uart();
    parse_multiboot();
    write_serial('b');
    init_mem();
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("hello\n");
    write_serial('c');
    init_screen();
    printf("fb: width %lu height %lu bpp %lu pitch %lu addr %08lX text %u\n",
           multiboot_fb_width, multiboot_fb_height, multiboot_fb_bpp,
           multiboot_fb_pitch, (uint32_t)multiboot_fb, multiboot_fb_text);
    write_serial('d');
    init_pit();
    init_kbd();
    rtc_init();
    printf("hello %08lX %08lX\n", magic, addr);
    enable_irq(2);
    asm("sti");
    printf("irq %02X %02X\n", inb(0x21), inb(0xA1));
    ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
    ide_read_sectors(0, 1, 0, mbr);
    if(mbr[446 + 4] == 0x83)
        ext2_init(&ext2_data, ((uint32_t *)(mbr + 446))[2]);
    else {
        printf("fat_mount %u\n", f_mount(&fat_data, "0:", 1));
        char buff[256];
        strcpy(buff, "0:");
        scan_files(buff);
        FATFS *fs;
        uint32_t fre_clust, fre_sect, tot_sect;
        f_getfree("0:", &fre_clust, &fs);
        // Get total sectors and free sectors
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;

        // Print the free space (assuming 512 bytes/sector)
        printf("%10lu KiB total drive space.\n%10lu KiB available.\n",
               tot_sect / 2, fre_sect / 2);

        FIL fil;
        UINT written;
        FRESULT res
          = f_open(&fil, "0:/test.txt", FA_READ | FA_WRITE | FA_OPEN_APPEND);
        if(res) goto fs_err;
        f_write(&fil, "abcd\n", 5, &written);
        printf("written %u\n", written);
        f_close(&fil);
    }
fs_err:
    printf("hello sin(0.25)=%f\n", sin(0.25));
    while(1)
        ;
}