#include "arch/cmos.h"
#include "arch/io.h"
#include "arch/intr.h"
#include <stdint.h>
#include <stdio.h>

#define BCD2BIN(x) ((((x) & 0xF0) >> 1) + (((x) & 0xF0) >> 3) + ((x) & 0xf))

uint32_t epoch = 0, fattime = 0;

uint32_t to_epoch(uint8_t s, uint8_t m, uint8_t h, uint8_t d, uint8_t mo, uint8_t y) {
    const uint16_t doy[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    uint32_t day = d + doy[mo - 1] - 1, year = y + 2000 - 1900;
    uint32_t res = s + m*60 + h*3600 + day*86400 +
        (year-70)*31536000 + ((year-69)/4)*86400 -
        ((year-1)/100)*86400 + ((year+299)/400)*86400;
    return res;
}

static void rtc_read(uint8_t *seconds, uint8_t *minutes, uint8_t *hours, uint8_t *day, uint8_t *month, uint8_t *year) {
    uint8_t buf[6], status;
    buf[0] = get_cmos(0x00);
    buf[1] = get_cmos(0x02);
    buf[2] = get_cmos(0x04);
    buf[3] = get_cmos(0x07);
    buf[4] = get_cmos(0x08);
    buf[5] = get_cmos(0x09);
    status = get_cmos(0x0B);
    if((status & 0x04) == 0) {
        buf[0] = BCD2BIN(buf[0]);
        buf[1] = BCD2BIN(buf[1]);
        buf[3] = BCD2BIN(buf[3]);
        buf[4] = BCD2BIN(buf[4]);
        buf[5] = BCD2BIN(buf[5]);
        if((status & 0x02) == 0) {
            if((buf[2] & 0x1F) == 0x12) buf[2] &= 0xE0;
            buf[2] = BCD2BIN(buf[2] & 0x1F);
            if(buf[2] & 0x80) buf[2] += 12;
        } else buf[2] = BCD2BIN(buf[2]);
    } else if((status & 0x02) == 0) {
        if((buf[2] & 0x0F) == 12) buf[2] &= 0xF0;
        if(buf[2] & 0x80) buf[2] = (buf[2] & 0x0F) + 12;
    }
    *seconds = buf[0];
    *minutes = buf[1];
    *hours = buf[2];
    *day = buf[3];
    *month = buf[4];
    *year = buf[5];
}

static void print_rtc(void) {
    // while(!rtc_is_update());
    // while(rtc_is_update());
    uint8_t data[6];
    rtc_read(&data[0], &data[1], &data[2], &data[3], &data[4], &data[5]);
    epoch = to_epoch(data[0], data[1], data[2], data[3], data[4], data[5]);
    fattime = ((data[5] + 2000 - 1980) << 25) | (data[4] << 21) | (data[3] << 16) | (data[2] << 11) | (data[1] << 5) | (data[0] >> 1);
    printf("%u:%02u:%02u %u.%02u.%02u %lu\n", data[2], data[1], data[0], data[3], data[4], data[5], epoch);
}

void do_irq8(void) {
    uint8_t status = get_cmos(0x0C);
    if(status & 0x10) print_rtc();
}

void rtc_init() {
    set_intr_gate(0x28, irq8);
    outb(0x70, 0x8B);
    uint8_t oldstate = inb(0x71);
    outb(0x70, 0x8B);
    outb(0x71, oldstate | 0x10);
    enable_irq(8);
}