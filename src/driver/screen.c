#include "drivers.h"
#include "arch/io.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define SCREEN_USE_TEXT
#define SCREEN_USE_GRAPHIC

#define SCREEN_BASE_ADDR 0xD0000000

#ifdef SCREEN_USE_TEXT
// #define SCREEN_BASE_ADDR 0xC00B8000

#define TEXT_NUM_COLS 80
#define TEXT_NUM_ROWS 25

struct TEXT_Char {
    uint8_t character;
    uint8_t color;
};

static void TEXT_cursor_move(uint8_t x, uint8_t y) { 
    uint16_t temp = (uint16_t)(y*TEXT_NUM_COLS + x); 
    outb(0x3D4, 14); 
    outb(0x3D5, temp >> 8); 
    outb(0x3D4, 15); 
    outb(0x3D5, temp); 
}
static void TEXT_cursor_enable(uint8_t cursor_start, uint8_t cursor_end) {
	outb(0x3D4, 0x0A);
	outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
 
	outb(0x3D4, 0x0B);
	outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}
static void TEXT_cursor_disable(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

static void TEXT_clear_row(drv_out_t *drv, size_t row) {
    drv_screen_text_data_t *data = (drv_screen_text_data_t*)drv->drv_data;
    struct TEXT_Char empty = {'\0', data->color};
    for(size_t col = 0; col < TEXT_NUM_COLS; col++)
        ((struct TEXT_Char*)data->buffer)[col + row * TEXT_NUM_COLS] = empty;
}
static void TEXT_newline(drv_out_t *drv) {
    drv_screen_text_data_t *data = (drv_screen_text_data_t*)drv->drv_data;
    data->col = 0;
    if(data->row < TEXT_NUM_ROWS - 1) {
        data->row++;
        return;
    }
    for(size_t row = 1; row < TEXT_NUM_ROWS; row++) {
        for(size_t col = 0; col < TEXT_NUM_COLS; col++)
            ((struct TEXT_Char*)data->buffer)[col + (row - 1) * TEXT_NUM_COLS] = ((struct TEXT_Char*)data->buffer)[col + row * TEXT_NUM_COLS];
    }
    TEXT_clear_row(drv, TEXT_NUM_ROWS - 1);
}

static void TEXT_screen_clear(drv_out_t *drv) {
    drv_screen_text_data_t *data = (drv_screen_text_data_t*)drv->drv_data;
    for(size_t i = 0; i < TEXT_NUM_ROWS; i++) TEXT_clear_row(drv, i);
    if(data->enabled) TEXT_cursor_move(0, 0);
    data->col = 0;
    data->row = 0;
}
static void TEXT_screen_char(drv_out_t *drv, char character) {
    drv_screen_text_data_t *data = (drv_screen_text_data_t*)drv->drv_data;
    if(character == '\n') {
        TEXT_newline(drv);
        if(data->enabled) TEXT_cursor_move((uint8_t)data->col, (uint8_t)data->row);
        return;
    }
    if(data->col >= TEXT_NUM_COLS) TEXT_newline(drv);
    ((struct TEXT_Char*)data->buffer)[data->col + data->row * TEXT_NUM_COLS] = (struct TEXT_Char) {(uint8_t) character, data->color};
    data->col++;
    if(data->enabled) TEXT_cursor_move((uint8_t)data->col, (uint8_t)data->row);
}
static void TEXT_screen_str(drv_out_t *drv, char* str) {
    for(size_t i = 0; 1; i++) {
        char character = str[i];
        if(character == '\0') return;
        TEXT_screen_char(drv, character);
    }
}
static void TEXT_screen_set_color(drv_out_t *drv, uint32_t foreground, uint32_t background) {
    drv_screen_text_data_t *data = (drv_screen_text_data_t*)drv->drv_data;
    data->color = (uint8_t)(foreground + (background << 4));
}

static void TEXT_screen_set_enabled(struct _driver_out_t *drv, uint8_t enable, void* buffer) {
    drv_screen_text_data_t *data = (drv_screen_text_data_t*)drv->drv_data;
    if(enable == 2) {
        data->enabled = 1;
        data->buffer = (void*)SCREEN_BASE_ADDR;
        TEXT_cursor_enable(data->cursor_start, data->cursor_end);
    } else if(enable == 1) {
        if(data->buffer != (void*)SCREEN_BASE_ADDR)
            for(uint32_t i = 0; i < TEXT_NUM_COLS * TEXT_NUM_ROWS; i++) ((uint16_t*)SCREEN_BASE_ADDR)[i] = ((uint16_t*)data->buffer)[i];
        data->enabled = 1;
        data->buffer = (void*)SCREEN_BASE_ADDR;
        TEXT_cursor_enable(data->cursor_start, data->cursor_end);
        TEXT_cursor_move((uint8_t)data->col, (uint8_t)data->row);
    } else {
        if(buffer != data->buffer)
            for(uint32_t i = 0; i < TEXT_NUM_COLS * TEXT_NUM_ROWS; i++) ((uint16_t*)buffer)[i] = ((uint16_t*)SCREEN_BASE_ADDR)[i];
        data->enabled = 0;
        data->buffer = buffer;
        TEXT_cursor_disable();
    }
}

void drv_screen_text_init(drv_out_t *drv) {
    drv_screen_text_data_t *data = (drv_screen_text_data_t*)drv->drv_data;
    data->color = OUT_COLOR_WHITE | (OUT_COLOR_BLACK << 4);
    data->col = 0;
    data->row = 0;
    data->enabled = 0;
    data->buffer = 0;
    drv->clear = TEXT_screen_clear;
    drv->ch = TEXT_screen_char;
    drv->str = TEXT_screen_str;
    drv->set_color = TEXT_screen_set_color;
    drv->set_enabled = TEXT_screen_set_enabled;
}
#endif
#ifdef SCREEN_USE_GRAPHIC

static inline void putchar_16bit(drv_screen_graphic_data_t *data, uint8_t c, uint16_t cx, uint16_t cy, uint16_t fg, uint16_t bg) {
    uint8_t bytesperline = (data->font->width+7) / 8;
    uint8_t *glyph = (uint8_t*)data->font + data->font->headersize + ((c > 0 && c < data->font->numglyph) ? c : 0) * data->font->bytesperglyph;
    uint32_t offs = (cy * data->font->height * data->pitch) + cx * (data->font->width * 2);
    uint32_t x, y, line, mask;
    for(y = 0; y < data->font->height; y++) {
        line = offs;
        mask = 1<<(data->font->width-1);
        for(x = 0; x < data->font->width; x++) {
            *((uint16_t*)(0xD0000000 + line)) = (*((uint32_t*)glyph) & mask) ? fg : bg;
            mask >>= 1;
            line += 2;
        }
        glyph += bytesperline;
        offs += data->pitch;
    }
}
static inline void putchar_24bit(drv_screen_graphic_data_t *data, uint8_t c, uint16_t cx, uint16_t cy, uint32_t fg, uint32_t bg) {
    uint8_t bytesperline = (data->font->width+7) / 8;
    uint8_t *glyph = (uint8_t*)data->font + data->font->headersize + ((c > 0 && c < data->font->numglyph) ? c : 0) * data->font->bytesperglyph;
    uint32_t offs = (cy * data->font->height * data->pitch) + (cx * data->font->width * 3);
    uint32_t x, y, line, mask;
    for(y = 0; y < data->font->height; y++){
        line = offs;
        mask = 1<<(data->font->width-1);
        for(x = 0; x < data->font->width; x++){
            *((uint32_t*)(0xD0000000 + line)) = (*((uint32_t*)glyph) & mask) ? (fg & 0xFFFFFF) : (bg & 0xFFFFFF) | (*((uint32_t*)(0xD0000000 + line)) & 0xFF000000);
            mask >>= 1;
            line += 3;
        }
        glyph += bytesperline;
        offs += data->pitch;
    }
}
static inline void putchar_32bit(drv_screen_graphic_data_t *data, uint8_t c, uint16_t cx, uint16_t cy, uint32_t fg, uint32_t bg) {
    uint8_t bytesperline = (data->font->width+7) / 8;
    uint8_t *glyph = (uint8_t*)data->font + data->font->headersize + ((c > 0 && c < data->font->numglyph) ? c : 0) * data->font->bytesperglyph;
    uint32_t offs = (cy * data->font->height * data->pitch) + (cx * data->font->width * 4);
    uint32_t x, y, line, mask;
    for(y = 0; y < data->font->height; y++){
        line = offs;
        mask = 1<<(data->font->width-1);
        for(x = 0; x < data->font->width; x++){
            *((uint32_t*)(0xD0000000 + line)) = (*((uint32_t*)glyph) & mask) ? fg : bg;
            mask >>= 1;
            line += 4;
        }
        glyph += bytesperline;
        offs += data->pitch;
    }
}
static void putchar_font(drv_screen_graphic_data_t *data, uint8_t c, uint16_t cx, uint16_t cy, uint32_t fg, uint32_t bg) {
    if(data->bpp == 32) putchar_32bit(data, c, cx, cy, fg, bg);
    else if(data->bpp == 24) putchar_24bit(data, c, cx, cy, fg & 0xFFFFFF, bg & 0xFFFFFF);
    else if(data->bpp == 16 || data->bpp == 15) putchar_16bit(data, c, cx, cy, fg & 0xFFFF, bg & 0xFFFF);
}

static void GRAPHIC_clear_row(drv_out_t *drv, size_t row) {
    drv_screen_graphic_data_t *data = drv->drv_data;
    for(size_t y = row * data->font->height; y < (row + 1) * data->font->height; y++) {
        uint8_t *bufr = (uint8_t*)data->buffer + (y * data->pitch);
        for(size_t x = 0; x < data->width; x++) {
            if(data->bpp == 32) *(uint32_t*)(bufr + x*4) = data->bg;
            else if(data->bpp == 32) {
                *(uint32_t*)(bufr + x*3) = data->bg | (*(uint32_t*)(bufr + x*3) & 0xFF000000);
            } else *(uint16_t*)(bufr + x*2) = data->bg;
        }
    }
}
static void GRAPHIC_newline(drv_out_t *drv) {
    drv_screen_graphic_data_t *data = drv->drv_data;
    data->col = 0;
    if(data->row < (data->height / data->font->height) - 1) {
        data->row++;
        return;
    }
    for(size_t y = 0; y < data->height - data->font->height; y++) {
        uint32_t i = y * data->pitch;
        memcpy((uint8_t*)data->buffer + i, (uint8_t*)data->buffer + (data->font->height * data->pitch) + i, data->pitch);
    }
    GRAPHIC_clear_row(drv, (data->height / data->font->height) - 1);
}
static void GRAPHIC_screen_clear(drv_out_t *drv) {
    drv_screen_graphic_data_t *data = drv->drv_data;
    for(size_t i = 0; i < (data->height / data->font->height); i++) GRAPHIC_clear_row(drv, i);
    // if(data->enabled) TEXT_cursor_move(0, 0);
    data->col = 0;
    data->row = 0;
}
static void GRAPHIC_screen_char(drv_out_t *drv, char character) {
    drv_screen_graphic_data_t *data = drv->drv_data;
    if(character == '\n') {
        GRAPHIC_newline(drv);
        // if(data->enabled) TEXT_cursor_move((uint8_t)data->col, (uint8_t)data->row);
        return;
    }
    // if(data->col >= (data->width / data->font->width)) GRAPHIC_newline(drv);
    putchar_font(data, character, data->col, data->row, data->fg, data->bg);
    data->col++;
    if(data->col >= (data->width / data->font->width)) GRAPHIC_newline(drv);
    // if(data->enabled) TEXT_cursor_move((uint8_t)data->col, (uint8_t)data->row);
}
static void GRAPHIC_screen_str(drv_out_t *drv, char* str) {
    for(size_t i = 0; 1; i++) {
        char character = str[i];
        if(character == '\0') return;
        GRAPHIC_screen_char(drv, character);
    }
}
static void GRAPHIC_screen_set_color(drv_out_t *drv, uint32_t foreground, uint32_t background) {
    drv_screen_graphic_data_t *data = drv->drv_data;
    if(data->bpp >= 24) {
        data->fg = foreground;
        data->bg = background;
    } else if(data->bpp == 16) {
        data->fg = ((foreground >> (3+16) & 0x3F) << 11) | ((foreground >> (2+8) & 0x7F) << 5) | (foreground >> 3 & 0x3F);
        data->bg = ((background >> (3+16) & 0x3F) << 11) | ((background >> (2+8) & 0x7F) << 5) | (background >> 3 & 0x3F);
    } else if(data->bpp == 15) {
        data->fg = ((foreground >> (3+16) & 0x3F) << 10) | ((foreground >> (3+8) & 0x3F) << 5) | (foreground >> 3 & 0x3F);
        data->bg = ((background >> (3+16) & 0x3F) << 10) | ((background >> (3+8) & 0x3F) << 5) | (background >> 3 & 0x3F);
    }
}

static void GRAPHIC_screen_set_enabled(struct _driver_out_t *drv, uint8_t enable, void* buffer) {
    drv_screen_graphic_data_t *data = drv->drv_data;
    if(enable == 2) {
        data->enabled = 1;
        data->buffer = (void*)SCREEN_BASE_ADDR;
        // TEXT_cursor_enable(data->cursor_start, data->cursor_end);
    } else if(enable == 1) {
        if(data->buffer != (void*)SCREEN_BASE_ADDR)
            memcpy((void*)SCREEN_BASE_ADDR, data->buffer, data->pitch * data->height);
        data->enabled = 1;
        data->buffer = (void*)SCREEN_BASE_ADDR;
        // TEXT_cursor_enable(data->cursor_start, data->cursor_end);
        // TEXT_cursor_move((uint8_t)data->col, (uint8_t)data->row);
    } else {
        if(buffer != data->buffer)
            memcpy(buffer, data->buffer, data->pitch * data->height);
        data->enabled = 0;
        data->buffer = buffer;
        // TEXT_cursor_disable();
    }
}

void drv_screen_graphic_init(drv_out_t *drv) {
    drv_screen_graphic_data_t *data = drv->drv_data;
    data->fg = 0;
    data->bg = 0;
    data->col = 0;
    data->row = 0;
    data->enabled = 0;
    data->buffer = 0;
    drv->clear = GRAPHIC_screen_clear;
    drv->ch = GRAPHIC_screen_char;
    drv->str = GRAPHIC_screen_str;
    drv->set_color = GRAPHIC_screen_set_color;
    drv->set_enabled = GRAPHIC_screen_set_enabled;
}
#endif