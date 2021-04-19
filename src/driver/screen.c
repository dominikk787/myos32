#include "driver_inout.h"
#include "io.h"
#include <stdint.h>
#include <stddef.h>

#define SCREEN_BASE_ADDR 0xC00B8000

#define NUM_COLS 80
#define NUM_ROWS 25

struct Char {
    uint8_t character;
    uint8_t color;
};

static void cursor_move(uint8_t x, uint8_t y) { 
    uint16_t temp = (y*NUM_COLS + x); 
    outportb(0x3D4, 14); 
    outportb(0x3D5, temp >> 8); 
    outportb(0x3D4, 15); 
    outportb(0x3D5, temp); 
}
static void cursor_enable(uint8_t cursor_start, uint8_t cursor_end) {
	outportb(0x3D4, 0x0A);
	outportb(0x3D5, (inportb(0x3D5) & 0xC0) | cursor_start);
 
	outportb(0x3D4, 0x0B);
	outportb(0x3D5, (inportb(0x3D5) & 0xE0) | cursor_end);
}
static void cursor_disable() {
    outportb(0x3D4, 0x0A);
    outportb(0x3D5, 0x20);
}

static void clear_row(drv_out_t *drv, size_t row) {
    drv_screen_data_t *data = (drv_screen_data_t*)drv->drv_data;
    struct Char empty = {'\0', data->color};
    for(size_t col = 0; col < NUM_COLS; col++)
        ((struct Char*)data->buffer)[col + row * NUM_COLS] = empty;
}
static void newline(drv_out_t *drv) {
    drv_screen_data_t *data = (drv_screen_data_t*)drv->drv_data;
    data->col = 0;
    if(data->row < NUM_ROWS - 1) {
        data->row++;
        return;
    }
    for(size_t row = 1; row < NUM_ROWS; row++) {
        for(size_t col = 0; col < NUM_COLS; col++)
            ((struct Char*)data->buffer)[col + (row - 1) * NUM_COLS] = ((struct Char*)data->buffer)[col + row * NUM_COLS];
    }
    clear_row(drv, NUM_ROWS - 1);
}

static void screen_clear(drv_out_t *drv) {
    drv_screen_data_t *data = (drv_screen_data_t*)drv->drv_data;
    for(size_t i = 0; i < NUM_ROWS; i++) clear_row(drv, i);
    if(data->enabled) cursor_move(0, 0);
    data->col = 0;
    data->row = 0;
}
static void screen_char(drv_out_t *drv, char character) {
    drv_screen_data_t *data = (drv_screen_data_t*)drv->drv_data;
    if(character == '\n') {
        newline(drv);
        if(data->enabled) cursor_move(data->col, data->row);
        return;
    }
    if(data->col >= NUM_COLS) newline(drv);
    ((struct Char*)data->buffer)[data->col + data->row * NUM_COLS] = (struct Char) {(uint8_t) character, data->color};
    data->col++;
    if(data->enabled) cursor_move(data->col, data->row);
}
static void screen_str(drv_out_t *drv, char* str) {
    for(size_t i = 0; 1; i++) {
        char character = str[i];
        if(character == '\0') return;
        screen_char(drv, character);
    }
}
static void screen_set_color(drv_out_t *drv, uint8_t foreground, uint8_t background) {
    drv_screen_data_t *data = (drv_screen_data_t*)drv->drv_data;
    data->color = foreground + (background << 4);
}

static void screen_set_enabled(struct _driver_out_t *drv, uint8_t enable, void* buffer) {
    drv_screen_data_t *data = (drv_screen_data_t*)drv->drv_data;
    if(enable == 2) {
        data->enabled = 1;
        data->buffer = (void*)SCREEN_BASE_ADDR;
        cursor_enable(data->cursor_start, data->cursor_end);
    } else if(enable == 1) {
        if(data->buffer != (void*)SCREEN_BASE_ADDR)
            for(uint32_t i = 0; i < NUM_COLS * NUM_ROWS; i++) ((uint16_t*)SCREEN_BASE_ADDR)[i] = ((uint16_t*)data->buffer)[i];
        data->enabled = 1;
        data->buffer = (void*)SCREEN_BASE_ADDR;
        cursor_enable(data->cursor_start, data->cursor_end);
        cursor_move(data->col, data->row);
    } else {
        if(buffer != data->buffer)
            for(uint32_t i = 0; i < NUM_COLS * NUM_ROWS; i++) ((uint16_t*)buffer)[i] = ((uint16_t*)SCREEN_BASE_ADDR)[i];
        data->enabled = 0;
        data->buffer = buffer;
        cursor_disable();
    }
}

void drv_screen_init(drv_out_t *drv) {
    drv_screen_data_t *data = (drv_screen_data_t*)drv->drv_data;
    data->color = OUT_COLOR_WHITE | (OUT_COLOR_BLACK << 4);
    data->col = 0;
    data->row = 0;
    data->enabled = 0;
    data->buffer = 0;
    drv->clear = screen_clear;
    drv->ch = screen_char;
    drv->str = screen_str;
    drv->set_color = screen_set_color;
    drv->set_enabled = screen_set_enabled;
}