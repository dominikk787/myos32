#pragma once
#include "ext2.h"

typedef struct __attribute__((packed)) {
    uint8_t JumpBoot[3];
    char FileSystemName[8];
    uint8_t MustBeZero[53];
    uint64_t PartitionOffset;
    uint64_t VolumeLength;
    uint32_t FatOffset;
    uint32_t FatLength;
    uint32_t ClusterHeapOffset;
    uint32_t ClusterCount;
    uint32_t FirstClusterOfRootDirectory;
    uint32_t VolumeSerialNumber;
    uint16_t FileSystemRevision;
    uint16_t VolumeFlags;
    uint8_t BytesPerSectorShift;
    uint8_t SectorsPerClusterShift;
    uint8_t NumberOfFats;
    uint8_t DriveSelect;
    uint8_t PercentInUse;
    uint8_t Reserved[7];
    uint8_t BootCode[390];
    uint16_t BootSignature;
} exfat_mbs_t;

typedef struct {
    uint8_t ParametersGuid[16];
    uint8_t CustomDefined[32];
} exfat_parameters_t;

typedef struct _driver_fs_t {
    uint32_t (*open)(struct _driver_fs_t *drv, const char *path);
    uint32_t (*resize)(struct _driver_fs_t *drv, uint32_t file, uint32_t size);
    uint32_t (*read)(struct _driver_fs_t *drv, uint32_t file, uint32_t sector, uint8_t *ptr);
    uint32_t (*write)(struct _driver_fs_t *drv, uint32_t file, uint32_t sector, uint8_t *ptr);
    uint32_t (*fstat)(struct _driver_fs_t *drv, uint32_t file, uint32_t *mode, uint32_t *size);
    uint32_t (*close)(struct _driver_fs_t *drv, uint32_t file);
    void *drv_data;
    uint32_t user_data;
} drv_fs_t;

typedef struct {
    ext2_sb_t sb;
    uint32_t num_bgds, fs_start, block_size, buf_block[2], bgdtbuf_block;
    uint8_t *buf[2], ready;
    ext2_bgdt_t *bgdtbuf;
} drv_fs_ext2_data_t;

typedef struct {
    exfat_mbs_t mbs;
    exfat_parameters_t parameters[10];
    uint32_t phys_sector, sector_cluster;
    uint8_t *buf, physbuf[512];
} drv_fs_exfat_data_t;

typedef struct {
    uint32_t code;
    drv_fs_t *fs;
    uint32_t mode;
    uint32_t offset;
    uint32_t size;
} vfs_file_t;

void ext2_init(drv_fs_ext2_data_t *data, uint32_t first_lba);
void ext2_print_sb(drv_fs_ext2_data_t *data);
void ext2_print_bgdt(drv_fs_ext2_data_t *data);
void ext2_print_inodes(drv_fs_ext2_data_t *data);
uint32_t ext2_find_inode(drv_fs_ext2_data_t *data, uint32_t start, const char* path);
void ext2_dump_inode(drv_fs_ext2_data_t *data, uint32_t i);

void exfat_init(drv_fs_exfat_data_t *data, uint32_t first_lba);