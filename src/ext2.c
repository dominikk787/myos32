#include "ext2.h"
#include "kernel.h"
#include "disk.h"

static ext2_sb_t sb;
static uint32_t num_bgds, bgdt_sector, fs_start, block_size;
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
    buf = kmalloc(block_size);
    if(!buf) kprint("ext2 kmalloc error\n");
}
void ext2_print_sb() {
    kprint("ext2 s_inodes_count %u\ns_blocks_count %u\ns_r_blocks_count %u\ns_free_blocks_count %u\n"
        "s_free_inodes_count %u\ns_first_data_block %u\ns_log_block_size %u\ns_log_frag_size %u\n",
        sb.s_inodes_count, sb.s_blocks_count, sb.s_r_blocks_count, sb.s_free_blocks_count,
        sb.s_free_inodes_count, sb.s_first_data_block, sb.s_log_block_size, sb.s_log_frag_size);
    kprint("s_blocks_per_group %u\ns_frags_per_group %u\ns_inodes_per_group %u\ns_mtime %u\ns_wtime %u\n"
        "s_mnt_count %u\ns_max_mnt_count %u\ns_magic %04X\ns_state %u\ns_errors %u\ns_minor_rev_level %u\n"
        "ns_rev_level %u\nnum_bgds %u\nbgdt_sector %u\nblock_size %u\n",
        sb.s_blocks_per_group, sb.s_frags_per_group, sb.s_inodes_per_group, sb.s_mtime, sb.s_wtime,
        sb.s_mnt_count, sb.s_max_mnt_count, sb.s_magic, sb.s_state, sb.s_errors, sb.s_minor_rev_level,
        sb.s_rev_level, num_bgds, bgdt_sector, block_size);
}
void ext2_print_bgdt() {
    uint32_t sectors = num_bgds * sizeof(ext2_bgdt_t);
    sectors = (sectors / 512) + ((sectors % 512) ? 1 : 0);
    kprint("read %u\n", ide_ata_access(0, 0, fs_start + bgdt_sector, sectors, 0x10, buf));
    ext2_bgdt_t *table = (void*)buf;
    for(uint32_t i = 0; i < num_bgds; i++) {
        kprint("bg %u\nbg_block_bitmap %u\nbg_inode_bitmap %u\nbg_inode_table %u\nbg_free_blocks_count %u\nbg_free_inodes_count %u\nbg_used_dirs_count %u\n",
            i, table[i].bg_block_bitmap, table[i].bg_inode_bitmap, table[i].bg_inode_table, table[i].bg_free_blocks_count,
            table[i].bg_free_inodes_count, table[i].bg_used_dirs_count);
    }
}
void ext2_print_inode() {
    ext2_bgdt_t grpbuf[512 / sizeof(ext2_bgdt_t)];
    uint32_t grp_buf_sector = 0xFFFFFFFF, last_block = 0xFFFFFFFF;
    for(uint32_t i = 1; i < sb.s_inodes_count; i++) {
        uint32_t group = (i - 1) / sb.s_inodes_per_group, index = ((i - 1) % sb.s_inodes_per_group) * (sb.s_rev_level > 0 ? sb.s_inode_size : 128);
        uint32_t group_sector = group / (512 / sizeof(ext2_bgdt_t));
        if(grp_buf_sector != group_sector) {
            ide_ata_access(0, 0, fs_start + bgdt_sector + group_sector, 1, 0x10, grpbuf);
            grp_buf_sector = group_sector;
        }
        group %= (512 / sizeof(ext2_bgdt_t));
        uint32_t block = index / block_size + grpbuf[group].bg_inode_table;
        index = index % block_size;
        if(block != last_block) {
            ide_ata_access(0, 0, fs_start + (block * (block_size / 512)), block_size / 512, 0x10, buf);
            last_block = block;
        }
        // kprint("inode %u %u\n", block, index);     
        ext2_inode_t *inode = (void*)buf + index;
        if(inode->i_links_count == 0 || inode->i_links_count == 0xFFFF) continue;
        if((inode->i_mode & 0xF000) != 0x8000)
            kprint("inode %u tp %04X links %u\n", i, inode->i_mode, inode->i_links_count);
    }
}