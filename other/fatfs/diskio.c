//-----------------------------------------------------------------------
// Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019
//-----------------------------------------------------------------------
// If a working storage control module is available, it should be
// attached to the FatFs via a glue function rather than modifying it.
// This is an example of glue functions to attach various exsisting
// storage control modules to the FatFs module with a defined API.
//-----------------------------------------------------------------------

#include "diskio.h"  // Declarations of disk functions

#include "arch/cmos.h"
#include "disk.h"
#include "ff.h"  // Obtains integer types

#include <stdio.h>

//-----------------------------------------------------------------------
// Get Drive Status
//-----------------------------------------------------------------------

DSTATUS disk_status(BYTE pdrv) {
    // printf("ds %u\n", pdrv);
    return 0;
}

//-----------------------------------------------------------------------
// Inidialize a Drive
//-----------------------------------------------------------------------

DSTATUS disk_initialize(BYTE pdrv) {
    // printf("di %u\n", pdrv);
    return 0;
}

//-----------------------------------------------------------------------
// Read Sector(s)
//-----------------------------------------------------------------------

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    // printf("dr %u %lX %lu\n", pdrv, sector, count);
    uint8_t status = ide_read_sectors(0, count, sector, buff);
    return (status == 0) ? RES_OK : RES_ERROR;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    // printf("dw %u %lX %lu\n", pdrv, sector, count);
    uint8_t status = ide_write_sectors(0, count, sector, buff);
    return (status == 0) ? RES_OK : RES_ERROR;
}

#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    // printf("di %u %u\n", pdrv, cmd);
    switch(cmd) {
    case CTRL_SYNC: return RES_OK; break;
    case GET_SECTOR_COUNT: *(LBA_t *)buff = ide_devices[0].Size; break;
    case GET_BLOCK_SIZE: *(WORD *)buff = 1; break;
    default: return RES_PARERR;
    }

    return RES_OK;
}

DWORD get_fattime(void) { return fattime; }