#include "ext2.h"
#include "kernel.h"
#include "disk.h"
#include <stdio.h>
#include <stdlib.h>

static ext2_sb_t sb;
static uint32_t num_bgds, bgdt_sector, fs_start, block_size, buf_block, invld;
static uint8_t *buf;

void ext2_init(uint32_t first_lba) {
    ide_ata_access(0, 0, first_lba + 2, 2, 0x10, &sb);
    uint32_t a, b;
    a = (sb.s_blocks_count / sb.s_blocks_per_group) + ((sb.s_blocks_count % sb.s_blocks_per_group) > 0 ? 1 : 0);
    b = (sb.s_inodes_count / sb.s_inodes_per_group) + ((sb.s_inodes_count % sb.s_inodes_per_group) > 0 ? 1 : 0);
    num_bgds = (a > b) ? a : b;
    bgdt_sector = (sb.s_log_block_size == 0) ? 2 : 1;
    bgdt_sector *= 2 << sb.s_log_block_size;
    fs_start = first_lba;
    block_size = 1024 << sb.s_log_block_size;
    buf = malloc(block_size);
    if(!buf) printf("ext2 kmalloc error\n");
    buf_block = 0xFFFFFFFF;
    invld = 1;
}
void ext2_print_sb() {
    printf("ext2 s_inodes_count %u\ns_blocks_count %u\ns_r_blocks_count %u\ns_free_blocks_count %u\n"
        "s_free_inodes_count %u\ns_first_data_block %u\ns_log_block_size %u\ns_log_frag_size %u\n",
        sb.s_inodes_count, sb.s_blocks_count, sb.s_r_blocks_count, sb.s_free_blocks_count,
        sb.s_free_inodes_count, sb.s_first_data_block, sb.s_log_block_size, sb.s_log_frag_size);
    printf("s_blocks_per_group %u\ns_frags_per_group %u\ns_inodes_per_group %u\ns_mtime %u\ns_wtime %u\n"
        "s_mnt_count %u\ns_max_mnt_count %u\ns_magic %04X\ns_state %u\ns_errors %u\ns_minor_rev_level %u\n"
        "ns_rev_level %u\nnum_bgds %u\nbgdt_sector %u\nblock_size %u\n",
        sb.s_blocks_per_group, sb.s_frags_per_group, sb.s_inodes_per_group, sb.s_mtime, sb.s_wtime,
        sb.s_mnt_count, sb.s_max_mnt_count, sb.s_magic, sb.s_state, sb.s_errors, sb.s_minor_rev_level,
        sb.s_rev_level, num_bgds, bgdt_sector, block_size);
}
void ext2_print_bgdt() {
    uint32_t sectors = num_bgds * sizeof(ext2_bgdt_t);
    sectors = (sectors / 512) + ((sectors % 512) ? 1 : 0);
    printf("read %u\n", ide_ata_access(0, 0, fs_start + bgdt_sector, sectors, 0x10, buf));
    buf_block = 0xFFFFFFFF;
    ext2_bgdt_t *table = (void*)buf;
    for(uint32_t i = 0; i < num_bgds; i++) {
        printf("bg %u\nbg_block_bitmap %u\nbg_inode_bitmap %u\nbg_inode_table %u\nbg_free_blocks_count %u\nbg_free_inodes_count %u\nbg_used_dirs_count %u\n",
            i, table[i].bg_block_bitmap, table[i].bg_inode_bitmap, table[i].bg_inode_table, table[i].bg_free_blocks_count,
            table[i].bg_free_inodes_count, table[i].bg_used_dirs_count);
    }
    invld = 1;
}
void ext2_print_inode(uint32_t i) {
    static ext2_bgdt_t grpbuf[512 / sizeof(ext2_bgdt_t)];
    static uint32_t grp_buf_sector = 0xFFFFFFFF;
    uint32_t group = (i - 1) / sb.s_inodes_per_group, index = ((i - 1) % sb.s_inodes_per_group) * (sb.s_rev_level > 0 ? sb.s_inode_size : 128);
    uint32_t group_sector = group / (512 / sizeof(ext2_bgdt_t));
    if(grp_buf_sector != group_sector || invld) {
        printf("rgs %u ", ide_ata_access(0, 0, fs_start + bgdt_sector + group_sector, 1, 0x10, grpbuf));
        grp_buf_sector = group_sector;
        invld = 0;
    }
    group %= (512 / sizeof(ext2_bgdt_t));
    uint32_t block = index / block_size + grpbuf[group].bg_inode_table;
    index = index % block_size;
    if(block != buf_block) {
        printf("rib %u ", ide_ata_access(0, 0, fs_start + (block * (block_size / 512)), block_size / 512, 0x10, buf));
        buf_block = block;
    }
    // printf("inode %u %u\n", block, index);     
    ext2_inode_t inode = *((ext2_inode_t*)(buf + index));
    // if(inode->i_links_count == 0 || inode->i_links_count == 0xFFFF) return;
    // if((inode->i_mode & 0xF000) != 0x8000)
        printf("inode %u tp %04X links %u ", i, inode.i_mode, inode.i_links_count);
    if((inode.i_mode & 0xF000) == 0x4000) {
        // printf("directory\n");
        for(uint8_t i = 0; i < 15; i++) if(inode.i_block[i]) {
            printf("block %u %u ", i, inode.i_block[i]);
            uint32_t offset = 0;
            while(offset < block_size) {
                if(buf_block != inode.i_block[i]) {
                    printf("rde %u ", ide_ata_access(0, 0, fs_start + (inode.i_block[i] * (block_size / 512)), block_size / 512, 0x10, buf));
                    buf_block = inode.i_block[i];
                }
                ext2_direntry_t *de = (void*)buf + offset;
                if(de->size == 0) break;
                uint8_t hidden = 0;
                if(de->str[0] == '.') {
                    if(de->str_len == 1) hidden = 1;
                    else if(de->str_len == 2 && de->str[1] == '.') hidden = 1;
                }
                if(!hidden) {
                    printf("de %u %02X %u %u ", de->inode, de->type, de->size, de->str_len);
                    for(uint8_t a = 0; a < de->str_len; a++) printf("%c", de->str[a]);
                    printf("\n");
                }
                offset += de->size;
                if(!hidden && de->type == 2) {
                    ext2_print_inode(de->inode);
                }
            }
        }
    }
}
void ext2_print_inodes() {
    ext2_print_inode(2);
}