#pragma once

#include <stdint.h>

extern struct ide_device {
    uint8_t Reserved;       // 0 (Empty) or 1 (This Drive really exists).
    uint8_t Channel;        // 0 (Primary Channel) or 1 (Secondary Channel).
    uint8_t Drive;          // 0 (Master Drive) or 1 (Slave Drive).
    uint16_t Type;          // 0: ATA, 1:ATAPI.
    uint16_t Signature;     // Drive Signature
    uint16_t Capabilities;  // Features.
    uint32_t CommandSets;   // Command Sets Supported.
    uint32_t Size;          // Size in Sectors.
    uint8_t Model[41];      // Model in string.
} ide_devices[4];

void ide_initialize(uint16_t BAR0, uint16_t BAR1, uint16_t BAR2, uint16_t BAR3,
                    uint16_t BAR4);
uint8_t ide_read_sectors(uint8_t drive, uint8_t numsects, uint32_t lba,
                         uint8_t *buf);
uint8_t ide_write_sectors(uint8_t drive, uint8_t numsects, uint32_t lba,
                          const uint8_t *buf);