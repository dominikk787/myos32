#include "ext2.h"
#include "kernel.h"
#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/driver_fs.h"

static void read_block_buf(drv_fs_ext2_data_t *data, uint8_t n, uint32_t block) {
    if(data->buf_block[n] == block) return;
    // printf("read%u block %lu\n", n, block);
    ide_read_sectors(0, (uint8_t)(data->block_size / 512), data->fs_start + (block * (data->block_size / 512)), data->buf[n]);
    data->buf_block[n] = block;
}
static void read_block_bgdt(drv_fs_ext2_data_t *data, uint32_t block) {
    if(data->bgdtbuf_block == block) return;
    // printf("readbgdt block %lu\n", block);
    ide_read_sectors(0, (uint8_t)(data->block_size / 512), data->fs_start + (block * (data->block_size / 512)), (void*)data->bgdtbuf);
    data->bgdtbuf_block = block;
}

void ext2_init(drv_fs_ext2_data_t *data, uint32_t first_lba) {
    ide_read_sectors(0, 2, first_lba + 2, (void*)&data->sb);
    data->ready = 0;
    if(data->sb.s_magic != 0xEF53) return;
    uint32_t a, b;
    a = (data->sb.s_blocks_count / data->sb.s_blocks_per_group) + ((data->sb.s_blocks_count % data->sb.s_blocks_per_group) > 0 ? 1 : 0);
    b = (data->sb.s_inodes_count / data->sb.s_inodes_per_group) + ((data->sb.s_inodes_count % data->sb.s_inodes_per_group) > 0 ? 1 : 0);
    data->num_bgds = (a > b) ? a : b;
    data->fs_start = first_lba;
    data->block_size = 1024 << data->sb.s_log_block_size;
    data->buf[0] = malloc(data->block_size);
    data->buf[1] = malloc(data->block_size);
    data->bgdtbuf = malloc(data->block_size);
    if(!data->buf[0] || !data->buf[1] || !data->bgdtbuf) printf("ext2 malloc error\n");
    data->buf_block[0] = 0xFFFFFFFF;
    data->buf_block[1] = 0xFFFFFFFF;
    data->bgdtbuf_block = 0xFFFFFFFF;
    data->ready = 1;
}
void ext2_print_sb(drv_fs_ext2_data_t *data) {
    if(!data->ready) return;
    printf("ext2 s_inodes_count %lu\ns_blocks_count %lu\ns_r_blocks_count %lu\ns_free_blocks_count %lu\n"
        "s_free_inodes_count %lu\ns_first_data_block %lu\ns_log_block_size %lu\ns_log_frag_size %lu\n",
        data->sb.s_inodes_count, data->sb.s_blocks_count, data->sb.s_r_blocks_count, data->sb.s_free_blocks_count,
        data->sb.s_free_inodes_count, data->sb.s_first_data_block, data->sb.s_log_block_size, data->sb.s_log_frag_size);
    printf("s_blocks_per_group %lu\ns_frags_per_group %lu\ns_inodes_per_group %lu\ns_mtime %lu\ns_wtime %lu\n"
        "s_mnt_count %u\ns_max_mnt_count %u\ns_magic %04X\ns_state %u\ns_errors %u\ns_minor_rev_level %u\n"
        "ns_rev_level %lu\nnum_bgds %lu\nblock_size %lu\n",
        data->sb.s_blocks_per_group, data->sb.s_frags_per_group, data->sb.s_inodes_per_group, data->sb.s_mtime, data->sb.s_wtime,
        data->sb.s_mnt_count, data->sb.s_max_mnt_count, data->sb.s_magic, data->sb.s_state, data->sb.s_errors, data->sb.s_minor_rev_level,
        data->sb.s_rev_level, data->num_bgds, data->block_size);
}
void ext2_print_bgdt(drv_fs_ext2_data_t *data) {
    if(!data->ready) return;
    uint32_t sectors = data->num_bgds * sizeof(ext2_bgdt_t);
    sectors = (sectors / 512) + ((sectors % 512) ? 1 : 0);
    read_block_bgdt(data, data->block_size == 1024 ? 2 : 1);
    for(uint32_t i = 0; i < data->num_bgds; i++) {
        printf("bg %lu\nbg_block_bitmap %lu\nbg_inode_bitmap %lu\nbg_inode_table %lu\nbg_free_blocks_count %u\nbg_free_inodes_count %u\nbg_used_dirs_count %u\n",
            i, data->bgdtbuf[i].bg_block_bitmap, data->bgdtbuf[i].bg_inode_bitmap, data->bgdtbuf[i].bg_inode_table, data->bgdtbuf[i].bg_free_blocks_count,
            data->bgdtbuf[i].bg_free_inodes_count, data->bgdtbuf[i].bg_used_dirs_count);
    }
}
static void ext2_print_inode(drv_fs_ext2_data_t *data, uint32_t i, const char *path) {
    uint32_t group = (i - 1) / data->sb.s_inodes_per_group, index = ((i - 1) % data->sb.s_inodes_per_group) * (data->sb.s_rev_level > 0 ? data->sb.s_inode_size : 128);
    uint32_t group_blk = group / (data->block_size / sizeof(ext2_bgdt_t)) + (data->block_size == 1024 ? 2 : 1);
    read_block_bgdt(data, group_blk);
    group %= (data->block_size / sizeof(ext2_bgdt_t));
    uint32_t block = index / data->block_size + data->bgdtbuf[group].bg_inode_table;
    index = index % data->block_size;
    read_block_buf(data, 0, block);
    // printf("group %lu:%lu block %lu\n", group, group_blk, block);
    // printf("inode %u %u\n", block, index);     
    ext2_inode_t inode;
    memcpy(&inode, data->buf[0] + index, sizeof(ext2_inode_t));
    if(inode.i_links_count == 0 || inode.i_links_count == 0xFFFF) return;
    // printf("inode %lu tp %04X links %u ", i, inode.i_mode, inode.i_links_count);
    // printf("inode %lu tp %04X\n", i, inode.i_mode);
    if((inode.i_mode & 0xF000) == 0x4000) {
        // printf("directory\n");
        // for(uint8_t j = 0; j < 15; j++) printf("found block %u %lu ", j, inode.i_block[j]);
        for(uint8_t j = 0; j < 12; j++) if(inode.i_block[j]) {
            // printf("block %u %lu ", j, inode.i_block[j]);
            uint32_t offset = 0;
            while(offset < data->block_size) {
                read_block_buf(data, 1, inode.i_block[j]);
                ext2_direntry_t *de = (void*)(data->buf[1] + offset);
                if(de->size == 0) {
                    offset = (offset + 3) & ~3;
                    printf(" %08lX", *(uint32_t*)(data->buf[1] + offset));
                    offset += 4;
                    continue;
                }
                if(de->str_len == 0) {
                    offset += de->size;
                    continue;
                }
                uint8_t hidden = 0;
                if(de->str[0] == '.') {
                    if(de->str_len == 1) hidden = 1;
                    else if(de->str_len == 2 && de->str[1] == '.') hidden = 1;
                }
                if(!hidden) {
                    printf("de %lu %02X %u %u %lu %s/", de->inode, de->type, de->size, de->str_len, offset, path);
                    for(uint8_t a = 0; a < de->str_len; a++) printf("%c", de->str[a]);
                    printf("\n");
                }
                offset += de->size;
                if(!hidden && de->type == 2) {
                    char *newpath = malloc(strlen(path) + de->str_len);
                    strcpy(newpath, path);
                    strcat(newpath, "/");
                    strncat(newpath, (char*)de->str, de->str_len);
                    ext2_print_inode(data, de->inode, newpath);
                    free(newpath);
                } else if(!hidden && de->type == 1) {
                    // ext2_print_inode(data, de->inode, path);
                }
            }
        }
    }
}
void ext2_print_inodes(drv_fs_ext2_data_t *data) {
    if(!data->ready) return;
    ext2_print_inode(data, 2, "");
}

static uint32_t find_in_dir_block(drv_fs_ext2_data_t *data, uint32_t block, const char* name) {
    if(block == 0) return 0;
    read_block_buf(data, 1, block);
    uint32_t offset = 0;
    while(offset < data->block_size) {
        ext2_direntry_t *de = (void*)(data->buf[1] + offset);
        if(de->size == 0) {
            offset = (offset + 3) & ~3;
            offset += 4;
            continue;
        }
        if(de->str_len == 0) {
            offset += de->size;
            continue;
        }
        if(strncmp((char*)de->str, name, de->str_len) == 0) return de->inode;
        offset += de->size;
    }
    return 0;
}
static uint32_t ext2_find_in_dir(drv_fs_ext2_data_t *data, uint32_t dirinode, const char* name) {
    uint32_t group = (dirinode - 1) / data->sb.s_inodes_per_group, 
        index = ((dirinode - 1) % data->sb.s_inodes_per_group) * (data->sb.s_rev_level > 0 ? data->sb.s_inode_size : 128);
    uint32_t group_blk = group / (data->block_size / sizeof(ext2_bgdt_t)) + (data->block_size == 1024 ? 2 : 1);
    read_block_bgdt(data, group_blk);
    group %= (data->block_size / sizeof(ext2_bgdt_t));
    uint32_t block = index / data->block_size + data->bgdtbuf[group].bg_inode_table;
    index = index % data->block_size;
    read_block_buf(data, 0, block);
    ext2_inode_t inode;
    memcpy(&inode, data->buf[0] + index, sizeof(ext2_inode_t));
    if(inode.i_links_count == 0 || inode.i_links_count == 0xFFFF || (inode.i_mode & 0xF000) != 0x4000) return 0;
    for(uint8_t j = 0; j < 12; j++) {
        if(inode.i_block[j] == 0) return 0;
        uint32_t found = find_in_dir_block(data, inode.i_block[j], name);
        if(found) return found;
    }
    if(inode.i_block[12]) {
        read_block_buf(data, 0, inode.i_block[12]);
        uint32_t *buf = (void*)data->buf[0];
        for(uint32_t j = 0; j < data->block_size / 4; j++) {
            if(buf[j] == 0) return 0;
            uint32_t found = find_in_dir_block(data, buf[j], name);
            if(found) return found;
        }
    } else return 0;
    return 0;
}
uint32_t ext2_find_inode(drv_fs_ext2_data_t *data, uint32_t start, const char* path) {
    if(!data->ready) return 0;
    if(path[0] == '/') path++;
    char *nextsep = memchr(path, '/', strlen(path));
    if(nextsep == NULL) {
        uint32_t inode = ext2_find_in_dir(data, start, path);
        printf("found %s %lu\n", path, inode);
        return inode;
    } else {
        uint32_t entrynamelen = nextsep - path;
        char *entryname = malloc(entrynamelen + 1);
        strncpy(entryname, path, entrynamelen);
        entryname[entrynamelen] = 0;
        uint32_t inode = ext2_find_in_dir(data, start, entryname);
        printf("looking for %s %lu\n", entryname, inode);
        free(entryname);
        return ext2_find_inode(data, inode, nextsep + 1);
    }
}

void ext2_dump_inode(drv_fs_ext2_data_t *data, uint32_t i) {
    if(!data->ready) return;
    uint32_t group = (i - 1) / data->sb.s_inodes_per_group, index = ((i - 1) % data->sb.s_inodes_per_group) * (data->sb.s_rev_level > 0 ? data->sb.s_inode_size : 128);
    uint32_t group_blk = group / (data->block_size / sizeof(ext2_bgdt_t)) + (data->block_size == 1024 ? 2 : 1);
    read_block_bgdt(data, group_blk);
    group %= (data->block_size / sizeof(ext2_bgdt_t));
    uint32_t block = index / data->block_size + data->bgdtbuf[group].bg_inode_table;
    index = index % data->block_size;
    read_block_buf(data, 0, block);   
    ext2_inode_t inode;
    memcpy(&inode, data->buf[0] + index, sizeof(ext2_inode_t));
    if(inode.i_links_count == 0 || inode.i_links_count == 0xFFFF) {
        printf("inode %lu unused\n", i);
        return;
    }
    printf("inode %lu\n", i);
    printf("mode %04X\n", inode.i_mode);
    printf("size %lu\n", inode.i_size);
    printf("uid %u\n", inode.i_uid);
    printf("gid %u\n", inode.i_gid);
    printf("links %u\n", inode.i_links_count);
    printf("blocks %lu\n", inode.i_blocks);
    printf("flags %08lX\n", inode.i_flags);
    for(uint8_t j = 0; j < 15; j++) printf("block%u %lu\n", j, inode.i_block[j]);
}

// static uint32_t ext2_open(drv_fs_t *drv, const char *path) {
//     return ext2_find_inode(drv->drv_data, 2, path);
// }
// static uint32_t ext2_close(drv_fs_t *drv, uint32_t file) {
//     return 0;
// }