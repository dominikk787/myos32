#pragma once

#include <stdint.h>

enum {
    IN_KEY_F1 = 0x80,
    IN_KEY_F2,
    IN_KEY_F3,
    IN_KEY_F4,
    IN_KEY_F5,
    IN_KEY_F6,
    IN_KEY_F7,
    IN_KEY_F8,
    IN_KEY_F9,
    IN_KEY_F10,
    IN_KEY_F11,
    IN_KEY_F12,
    IN_KEY_INS = 0x90,
    IN_KEY_DEL,
    IN_KEY_HOME,
    IN_KEY_END,
    IN_KEY_PGUP,
    IN_KEY_PGDN,
    IN_KEY_LFT,
    IN_KEY_UP,
    IN_KEY_DN,
    IN_KEY_RT,
    IN_KEY_PRNT,
    IN_KEY_PAUSE,
    IN_KEY_LWIN,
    IN_KEY_RWIN,
    IN_KEY_MENU
};

#define IN_SPECIAL_ALT 0x0200 
#define IN_SPECIAL_CTRL 0x0400 
#define IN_SPECIAL_SHIFT 0x0800 
#define IN_SPECIAL_ANY (IN_SPECIAL_ALT | IN_SPECIAL_CTRL | IN_SPECIAL_SHIFT)

enum {
    OUT_COLOR_BLACK = 0,
	OUT_COLOR_BLUE = 1,
	OUT_COLOR_GREEN = 2,
	OUT_COLOR_CYAN = 3,
	OUT_COLOR_RED = 4,
	OUT_COLOR_MAGENTA = 5,
	OUT_COLOR_BROWN = 6,
	OUT_COLOR_LIGHT_GRAY = 7,
	OUT_COLOR_DARK_GRAY = 8,
	OUT_COLOR_LIGHT_BLUE = 9,
	OUT_COLOR_LIGHT_GREEN = 10,
	OUT_COLOR_LIGHT_CYAN = 11,
	OUT_COLOR_LIGHT_RED = 12,
	OUT_COLOR_PINK = 13,
	OUT_COLOR_YELLOW = 14,
	OUT_COLOR_WHITE = 15,
};

#define PSF_FONT_MAGIC 0x864ab572
typedef struct {
    uint32_t magic;         // magic bytes to identify PSF 
    uint32_t version;       // zero 
    uint32_t headersize;    // offset of bitmaps in file, 32 
    uint32_t flags;         // 0 if there's no unicode table 
    uint32_t numglyph;      // number of glyphs
    uint32_t bytesperglyph; // size of each glyph 
    uint32_t height;        // height in pixels 
    uint32_t width;         // width in pixels 
} PSF_font;

struct _driver_in_t;
struct _driver_out_t;
struct _driver_inout_t;

typedef struct _driver_in_t {
    void (*in_clb)(struct _driver_in_t *data, uint8_t ch, uint16_t flags);
    void *drv_data;
    uint32_t user_data;
    struct _driver_inout_t *inout;
} drv_in_t;

typedef struct _driver_out_t {
    void (*clear)(struct _driver_out_t *drv);
    void (*ch)(struct _driver_out_t *drv, char character);
    void (*str)(struct _driver_out_t *drv, char* string);
    void (*set_color)(struct _driver_out_t *drv, uint32_t foreground, uint32_t background);
    void (*set_enabled)(struct _driver_out_t *drv, uint8_t enable, void* buffer);
    void *drv_data;
    uint32_t user_data;
    struct _driver_inout_t *inout;
} drv_out_t;

typedef struct _driver_inout_t {
    drv_in_t *in;
    drv_out_t *out;
    void *user;
} drv_inout_t;


typedef struct {
    uint32_t col;
    uint32_t row;
    uint8_t color;
    uint8_t cursor_start;
    uint8_t cursor_end;
    uint8_t enabled;
    void* buffer;
} drv_screen_text_data_t;
typedef struct {
    uint32_t col;
    uint32_t row;
    uint32_t fg;
    uint32_t bg;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    const PSF_font *font;
    uint8_t cursor_start;
    uint8_t cursor_end;
    uint8_t enabled;
    void* buffer;
} drv_screen_graphic_data_t;

typedef struct {
    uint16_t saw_break_code, kbd_status;
} drv_kbd_data_t;

void drv_screen_text_init(drv_out_t *drv);
void drv_screen_graphic_init(drv_out_t *drv);
void drv_kbd_init(drv_in_t *drv);