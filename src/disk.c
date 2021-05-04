#include "disk.h"

#include "arch/io.h"
#include "kernel.h"

#include <stdint.h>
#include <stdio.h>

#define ATA_SR_BSY  0x80  // Busy
#define ATA_SR_DRDY 0x40  // Drive ready
#define ATA_SR_DF   0x20  // Drive write fault
#define ATA_SR_DSC  0x10  // Drive seek complete
#define ATA_SR_DRQ  0x08  // Data request ready
#define ATA_SR_CORR 0x04  // Corrected data
#define ATA_SR_IDX  0x02  // Index
#define ATA_SR_ERR  0x01  // Error

#define ATA_ER_BBK   0x80  // Bad block
#define ATA_ER_UNC   0x40  // Uncorrectable data
#define ATA_ER_MC    0x20  // Media changed
#define ATA_ER_IDNF  0x10  // ID mark not found
#define ATA_ER_MCR   0x08  // Media change request
#define ATA_ER_ABRT  0x04  // Command aborted
#define ATA_ER_TK0NF 0x02  // Track 0 not found
#define ATA_ER_AMNF  0x01  // No address mark

#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_READ_DMA        0xC8
#define ATA_CMD_READ_DMA_EXT    0x25
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_WRITE_DMA       0xCA
#define ATA_CMD_WRITE_DMA_EXT   0x35
#define ATA_CMD_CACHE_FLUSH     0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_PACKET          0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_IDENTIFY        0xEC

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

#define IDE_ATA   0x00
#define IDE_ATAPI 0x01

#define ATA_MASTER 0x00
#define ATA_SLAVE  0x01

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

// Channels:
#define ATA_PRIMARY   0x00
#define ATA_SECONDARY 0x01

// Directions:
#define ATA_READ  0x00
#define ATA_WRITE 0x01

static struct IDEChannelRegisters {
    uint16_t base;   // I/O Base.
    uint16_t ctrl;   // Control Base
    uint16_t bmide;  // Bus Master IDE
    uint8_t nIEN;    // nIEN (No Interrupt);
} channels[2];

struct ide_device ide_devices[4];

static uint8_t ide_buf[2048] = {0};

static uint8_t ide_read(uint8_t channel, uint8_t reg);
static void ide_write(uint8_t channel, uint8_t reg, uint8_t data);
static void ide_read_buffer(uint8_t channel, uint8_t reg, uint32_t buffer,
                            uint32_t quads);
static uint8_t ide_polling(uint8_t channel, uint8_t advanced_check);
static uint8_t ide_print_error(uint32_t drive, uint8_t err);
static uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint32_t lba,
                              uint8_t numsects, uint8_t *buf);

static uint8_t ide_read(uint8_t channel, uint8_t reg) {
    // printf("ir %02x %02x    ", (uint32_t)channel, (uint32_t) reg);
    uint8_t result;
    if(reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if(reg < 0x08) result = inb(channels[channel].base + reg);
    else if(reg < 0x0C)
        result = inb(channels[channel].base + (uint16_t)(reg - 0x06));
    else if(reg < 0x0E)
        result = inb(channels[channel].ctrl + (uint16_t)(reg - 0x0A));
    else if(reg < 0x16)
        result = inb(channels[channel].bmide + (uint16_t)(reg - 0x0E));
    if(reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    return result;
}

static void ide_write(uint8_t channel, uint8_t reg, uint8_t data) {
    // printf("iw %02x %02x %02x    ", (uint32_t)channel, (uint32_t)reg,
    // (uint32_t)data);
    if(reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if(reg < 0x08) outb(channels[channel].base + reg - 0x00, data);
    else if(reg < 0x0C)
        outb(channels[channel].base + reg - 0x06, data);
    else if(reg < 0x0E)
        outb(channels[channel].ctrl + reg - 0x0A, data);
    else if(reg < 0x16)
        outb(channels[channel].bmide + reg - 0x0E, data);
    if(reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

static void ide_read_buffer(uint8_t channel, uint8_t reg, uint32_t buffer,
                            uint32_t quads) {
    /* WARNING: This code contains a serious bug. The inline assembly trashes ES
     * and ESP for all of the code the compiler generates between the inline
     *           assembly blocks.
     */
    if(reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    asm("push es\n mov ax, ds\n mov es, eax");
    if(reg < 0x08) insd(channels[channel].base + reg - 0x00, buffer, quads);
    else if(reg < 0x0C)
        insd(channels[channel].base + reg - 0x06, buffer, quads);
    else if(reg < 0x0E)
        insd(channels[channel].ctrl + reg - 0x0A, buffer, quads);
    else if(reg < 0x16)
        insd(channels[channel].bmide + reg - 0x0E, buffer, quads);
    asm("pop es");
    if(reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

static uint8_t ide_polling(uint8_t channel, uint8_t advanced_check) {
    asm("");
    // (I) Delay 400 nanosecond for BSY to be set:
    // -------------------------------------------------
    ide_read(channel, ATA_REG_ALTSTATUS);  // Reading the Alternate Status port
                                           // wastes 100ns; loop four times.
    ide_read(channel, ATA_REG_ALTSTATUS);
    ide_read(channel, ATA_REG_ALTSTATUS);
    ide_read(channel, ATA_REG_ALTSTATUS);
    // for(uint8_t i = 0; i < 4; i++) {
    //     ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    //     (void)inportb(channels[channel].base  + ATA_REG_ALTSTATUS - 0x06);
    // }

    // (II) Wait for BSY to be cleared:
    // -------------------------------------------------
    // while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY) ; // Wait for BSY
    // to be zero.
poll_loop:
    asm goto("in al,dx\ntest al,cl\njnz %l2" ::"d"(channels[channel].base
                                                   + ATA_REG_STATUS),
             "c"(ATA_SR_BSY)
             : "al"
             : poll_loop);
    if(advanced_check) {
        uint8_t state
          = ide_read(channel, ATA_REG_STATUS);  // Read Status Register.

        // (III) Check For Errors:
        // -------------------------------------------------
        if(state & ATA_SR_ERR) return 2;  // Error.

        // (IV) Check If Device fault:
        // -------------------------------------------------
        if(state & ATA_SR_DF) return 1;  // Device Fault.

        // (V) Check DRQ:
        // -------------------------------------------------
        // BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
        if((state & ATA_SR_DRQ) == 0) return 3;  // DRQ should be set
        // while ((ide_read(channel, ATA_REG_STATUS) & ATA_SR_DRQ) == 0) ;
    }
    return 0;  // No Error.
}

static uint8_t ide_print_error(uint32_t drive, uint8_t err) {
    if(err == 0) return err;

    printf("IDE:");
    if(err == 1) {
        printf("- Device Fault\n     ");
        err = 19;
    } else if(err == 2) {
        uint8_t st = ide_read(ide_devices[drive].Channel, ATA_REG_ERROR);
        if(st & ATA_ER_AMNF) {
            printf("- No Address Mark Found\n     ");
            err = 7;
        }
        if(st & ATA_ER_TK0NF) {
            printf("- No Media or Media Error\n     ");
            err = 3;
        }
        if(st & ATA_ER_ABRT) {
            printf("- Command Aborted\n     ");
            err = 20;
        }
        if(st & ATA_ER_MCR) {
            printf("- No Media or Media Error\n     ");
            err = 3;
        }
        if(st & ATA_ER_IDNF) {
            printf("- ID mark not Found\n     ");
            err = 21;
        }
        if(st & ATA_ER_MC) {
            printf("- No Media or Media Error\n     ");
            err = 3;
        }
        if(st & ATA_ER_UNC) {
            printf("- Uncorrectable Data Error\n     ");
            err = 22;
        }
        if(st & ATA_ER_BBK) {
            printf("- Bad Sectors\n     ");
            err = 13;
        }
    } else if(err == 3) {
        printf("- Reads Nothing\n     ");
        err = 23;
    } else if(err == 4) {
        printf("- Write Protected\n     ");
        err = 8;
    }
    printf(
      "- [%s %s] %s\n",
      (const char *[]){
        "Primary",
        "Secondary"}[ide_devices[drive].Channel],  // Use the channel as an
                                                   // index into the array
      (const char *[]){
        "Master",
        "Slave"}[ide_devices[drive].Drive],  // Same as above, using the drive
      ide_devices[drive].Model);

    return err;
}

void ide_initialize(uint16_t BAR0, uint16_t BAR1, uint16_t BAR2, uint16_t BAR3,
                    uint16_t BAR4) {
    uint8_t k, count = 0;

    // 1- Detect I/O Ports which interface IDE Controller:
    channels[ATA_PRIMARY].base
      = (uint16_t)(BAR0 & 0xFFFFFFFC);  // default 0x1F0
    channels[ATA_PRIMARY].ctrl
      = (uint16_t)(BAR1 & 0xFFFFFFFC);  // default 0x3F6
    channels[ATA_SECONDARY].base
      = (uint16_t)(BAR2 & 0xFFFFFFFC);  // default 0x170
    channels[ATA_SECONDARY].ctrl
      = (uint16_t)(BAR3 & 0xFFFFFFFC);  // default 0x376
    channels[ATA_PRIMARY].bmide
      = (uint16_t)(BAR4 & 0xFFFFFFFC) + 0;  // Bus Master IDE
    channels[ATA_SECONDARY].bmide
      = (uint16_t)(BAR4 & 0xFFFFFFFC) + 8;  // Bus Master IDE

    // 2- Disable IRQs:
    ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
    ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);

    // 3- Detect ATA-ATAPI Devices:
    for(uint8_t i = 0; i < 2; i++)
        for(uint8_t j = 0; j < 2; j++) {
            uint8_t err = 0, type = IDE_ATA;
            volatile uint8_t status;
            ide_devices[count].Reserved = 0;  // Assuming that no drive here.

            // (I) Select Drive:
            ide_write(i, ATA_REG_HDDEVSEL,
                      (uint8_t)(0xA0 | (j << 4)));  // Select Drive.
            // sleep(1); // Wait 1ms for drive select to work.
            {
                uint32_t c = ms_counter + 1;
                while(c >= ms_counter)
                    ;
            }

            // (II) Send ATA Identify Command:
            ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            // sleep(1); // This function should be implemented in your OS.
            // which waits for 1 ms. it is based on System Timer Device Driver.
            {
                uint32_t c = ms_counter + 1;
                while(c >= ms_counter)
                    ;
            }

            // (III) Polling:
            if(ide_read(i, ATA_REG_STATUS) == 0)
                continue;  // If Status = 0, No Device.

            while(1) {
                status = ide_read(i, ATA_REG_STATUS);
                if((status & ATA_SR_ERR)) {
                    err = 1;
                    break;
                }  // If Err, Device is not ATA.
                if(!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
                    break;  // Everything is right.
            }

            // (IV) Probe for ATAPI Devices:

            if(err != 0) {
                uint8_t cl = ide_read(i, ATA_REG_LBA1);
                uint8_t ch = ide_read(i, ATA_REG_LBA2);

                if(cl == 0x14 && ch == 0xEB) type = IDE_ATAPI;
                else if(cl == 0x69 && ch == 0x96)
                    type = IDE_ATAPI;
                else
                    continue;  // Unknown Type (may not be a device).

                ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
                {
                    uint32_t c = ms_counter + 1;
                    while(c >= ms_counter)
                        ;
                }
            }

            // (V) Read Identification Space of the Device:
            ide_read_buffer(i, ATA_REG_DATA, (unsigned int)ide_buf, 128);

            // (VI) Read Device Parameters:
            ide_devices[count].Reserved = 1;
            ide_devices[count].Type = type;
            ide_devices[count].Channel = i;
            ide_devices[count].Drive = j;
            ide_devices[count].Signature
              = *((uint16_t *)(ide_buf + ATA_IDENT_DEVICETYPE));
            ide_devices[count].Capabilities
              = *((uint16_t *)(ide_buf + ATA_IDENT_CAPABILITIES));
            ide_devices[count].CommandSets
              = *((uint32_t *)(ide_buf + ATA_IDENT_COMMANDSETS));

            // (VII) Get Size:
            if(ide_devices[count].CommandSets & (1 << 26))
                // Device uses 48-Bit Addressing:
                ide_devices[count].Size
                  = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
            else
                // Device uses CHS or 28-bit Addressing:
                ide_devices[count].Size
                  = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA));

            // (VIII) String indicates model of device (like Western Digital HDD
            // and SONY DVD-RW...):
            for(k = 0; k < 40; k += 2) {
                ide_devices[count].Model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
                ide_devices[count].Model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
            }
            ide_devices[count].Model[40] = 0;  // Terminate String.

            count++;
        }

    // 4- Print Summary:
    for(uint8_t i = 0; i < 4; i++)
        if(ide_devices[i].Reserved == 1) {
            printf(
              " Found %s Drive %ldMB - %s %04X\n",
              (const char *[]){"ATA", "ATAPI"}[ide_devices[i].Type], /* Type */
              ide_devices[i].Size / 1024 / 2,                        /* Size */
              ide_devices[i].Model, ide_devices[i].Capabilities);
        }
}

static uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint32_t lba,
                              uint8_t numsects, uint8_t *buf) {
    uint8_t lba_mode /* 0: CHS, 1:LBA28, 2: LBA48 */,
      dma /* 0: No DMA, 1: DMA */, cmd;
    uint8_t lba_io[6];
    uint8_t channel = ide_devices[drive].Channel;  // Read the Channel.
    uint32_t slavebit
      = ide_devices[drive].Drive;  // Read the Drive [Master/Slave]
    uint32_t bus = channels[channel]
                     .base;  // Bus Base, like 0x1F0 which is also data port.
    // uint32_t words = 256; // Almost every ATA drive has a sector-size of
    // 512-byte.
    uint16_t cyl;
    uint8_t head, sect, err;

    ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = 1 + 0x02);
    // printf("capabilities %04X\n", ide_devices[drive].Capabilities);

    // (I) Select one from LBA28, LBA48 or CHS;
    if(lba >= 0x10000000) {  // Sure Drive should support LBA in this case, or
                             // you are giving a wrong LBA.
        // LBA48:
        lba_mode = 2;
        lba_io[0] = (uint8_t)((lba & 0x000000FF) >> 0);
        lba_io[1] = (uint8_t)((lba & 0x0000FF00) >> 8);
        lba_io[2] = (uint8_t)((lba & 0x00FF0000) >> 16);
        lba_io[3] = (uint8_t)((lba & 0xFF000000) >> 24);
        lba_io[4]
          = 0;  // LBA28 is integer, so 32-bits are enough to access 2TB.
        lba_io[5]
          = 0;     // LBA28 is integer, so 32-bits are enough to access 2TB.
        head = 0;  // Lower 4-bits of HDDEVSEL are not used here.
        printf("ide lba48\n");
    } else if(ide_devices[drive].Capabilities & 0x200) {  // Drive supports LBA?
        // LBA28:
        lba_mode = 1;
        lba_io[0] = (lba & 0x00000FF) >> 0;
        lba_io[1] = (lba & 0x000FF00) >> 8;
        lba_io[2] = (lba & 0x0FF0000) >> 16;
        lba_io[3] = 0;  // These Registers are not used here.
        lba_io[4] = 0;  // These Registers are not used here.
        lba_io[5] = 0;  // These Registers are not used here.
        head = (lba & 0xF000000) >> 24;
        // printf("ide lba28\n");
    } else {
        // CHS:
        lba_mode = 0;
        sect = (lba & 0x3F) + 1;
        cyl = (uint16_t)((lba + 1 - sect) / (16 * 63));
        lba_io[0] = sect;
        lba_io[1] = (uint8_t)(cyl & 0xFF);
        lba_io[2] = (uint8_t)((cyl >> 8) & 0xFF);
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = (uint8_t)(
          (lba + 1 - sect) % (16 * 63)
          / (63));  // Head number is written to HDDEVSEL lower 4-bits.
        printf("ide chs\n");
    }

    // (II) See if drive supports DMA or not;
    dma = 0;  // We don't support DMA

    // (III) Wait if the drive is busy;
    // while ((ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY) == ATA_SR_BSY) ;
    // // Wait if busy.
access_loop:
    asm goto("in al,dx\ntest al,cl\njnz %l2" ::"d"(channels[channel].base
                                                   + ATA_REG_STATUS),
             "c"(ATA_SR_BSY)
             : "al"
             : access_loop);

    // (IV) Select Drive from the controller;
    if(lba_mode == 0)
        ide_write(channel, ATA_REG_HDDEVSEL,
                  (uint8_t)(0xA0 | (slavebit << 4) | head));  // Drive & CHS.
    else
        ide_write(channel, ATA_REG_HDDEVSEL,
                  (uint8_t)(0xE0 | (slavebit << 4) | head));  // Drive & LBA

    // (V) Write Parameters;
    if(lba_mode == 2) {
        ide_write(channel, ATA_REG_SECCOUNT1, 0);
        ide_write(channel, ATA_REG_LBA3, lba_io[3]);
        ide_write(channel, ATA_REG_LBA4, lba_io[4]);
        ide_write(channel, ATA_REG_LBA5, lba_io[5]);
    }
    ide_write(channel, ATA_REG_SECCOUNT0, numsects);
    ide_write(channel, ATA_REG_LBA0, lba_io[0]);
    ide_write(channel, ATA_REG_LBA1, lba_io[1]);
    ide_write(channel, ATA_REG_LBA2, lba_io[2]);

    // (VI) Select the command and send it;
    // Routine that is followed:
    // If ( DMA & LBA48)   DO_DMA_EXT;
    // If ( DMA & LBA28)   DO_DMA_LBA;
    // If ( DMA & LBA28)   DO_DMA_CHS;
    // If (!DMA & LBA48)   DO_PIO_EXT;
    // If (!DMA & LBA28)   DO_PIO_LBA;
    // If (!DMA & !LBA#)   DO_PIO_CHS;
    if(lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if(lba_mode == 1 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if(lba_mode == 2 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;
    if(lba_mode == 0 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if(lba_mode == 1 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if(lba_mode == 2 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
    if(lba_mode == 0 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if(lba_mode == 1 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if(lba_mode == 2 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
    if(lba_mode == 0 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if(lba_mode == 1 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if(lba_mode == 2 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
    ide_write(channel, ATA_REG_COMMAND, cmd);  // Send the Command.

    if(dma) return 4;
    // if (direction == 0);
    //     // DMA Read.
    // else;
    //     // DMA Write.
    else if(direction == 0)
        // PIO Read.
        for(uint8_t i = 0; i < numsects; i++) {
            if((err = ide_polling(channel, 1)) != 0) {
                printf("poll err %u\n", err);
                return err;  // Polling, set error and exit if there is.
            }
            // asm("pushw %es");
            // asm("mov %%ax, %%es" : : "a"(selector));
            // printf("reading sector %u\n", i);
            asm("rep insw" ::"c"(256), "d"(bus), "D"(buf),
                "a"(i));  // Receive Data
                          // asm("popw %es");
                          // buf += (256*2);
        }
    else {
        // PIO Write.
        for(uint8_t i = 0; i < numsects; i++) {
            ide_polling(channel, 0);  // Polling.
            // asm("pushw %ds");
            // asm("mov %%ax, %%ds"::"a"(selector));
            asm("rep outsw" ::"c"(256), "d"(bus),
                "S"((uint32_t)buf));  // Send Data
                                      // asm("popw %ds");
                                      // buf += (256*2);
        }
        ide_write(channel, ATA_REG_COMMAND,
                  (uint8_t[]){ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH,
                              ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);
        ide_polling(channel, 0);  // Polling.
    }

    return 0;  // Easy, isn't it?
}

uint8_t ide_read_sectors(uint8_t drive, uint8_t numsects, uint32_t lba,
                         uint8_t *buf) {
    if(drive > 3 || ide_devices[drive].Reserved == 0)
        return 1;  // Drive Not Found!
    if(((lba + numsects) > ide_devices[drive].Size)
       && (ide_devices[drive].Type == IDE_ATA))
        return 2;  // Seeking to invalid position.
    uint8_t err = 0;
    if(ide_devices[drive].Type == IDE_ATA)
        err = ide_ata_access(ATA_READ, drive, lba, numsects, buf);
    else if(ide_devices[drive].Type == IDE_ATAPI)
        return 3;  // Not Implemented.
    else
        return 1;
    return ide_print_error(drive, err);
}

uint8_t ide_write_sectors(uint8_t drive, uint8_t numsects, uint32_t lba,
                          const uint8_t *buf) {
    if(drive > 3 || ide_devices[drive].Reserved == 0)
        return 1;  // Drive Not Found!
    if(((lba + numsects) > ide_devices[drive].Size)
       && (ide_devices[drive].Type == IDE_ATA))
        return 2;  // Seeking to invalid position.
    uint8_t err = 0;
    if(ide_devices[drive].Type == IDE_ATA)
        err = ide_ata_access(ATA_WRITE, drive, lba, numsects, (void *)buf);
    else if(ide_devices[drive].Type == IDE_ATAPI)
        err = 4;  // Write-Protected.
    else
        return 1;
    return ide_print_error(drive, err);
}