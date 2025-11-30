import type.magic;
import std.core;
import type.time;

#pragma endian little
//#pragma debug

#define BLOCK_SIZE 1024
#define BITS_PER_BLOCK 8192

#define COLOR_UNUSED "404040"

struct BootLoader {
    u8 boot_loader_reserved[BLOCK_SIZE];
} [[color(COLOR_UNUSED)]];

struct SuperBlock {
    type::Magic<"\x42\x30\x20\x10"> magic [[comment("Magic Number.")]];
    u32 size           [[comment("Size in blocks.")]];
    u32 nblocks        [[comment("# Data blocks.")]];
    u32 ninode_blocks  [[comment("# Inode blocks.")]];
    u32 nlog           [[comment("# Log blocks.")]];
    u32 logstart       [[comment("Log start block.")]];
    u32 inodestart     [[comment("Inode start block.")]];
    u32 bmapstart      [[comment("Bitmap start block.")]];
    u8 sb_reserved[992] [[comment("Fill up to 1 block."), color(COLOR_UNUSED)]];
};

BootLoader boot_loader_reserved @ 0x00;

SuperBlock sb @ BLOCK_SIZE;

struct LogHeader {
    u32 n;
    u32 addr[sb.nlog];
    u32 unused[BLOCK_SIZE/4 - sb.nlog - 1] [[color(COLOR_UNUSED)]];
};

struct LogBlock {
    u8 data[BLOCK_SIZE];
};

LogHeader log @ sb.logstart * BLOCK_SIZE;
LogBlock log_block[sb.nlog-1] @ (sb.logstart + 1) * BLOCK_SIZE;

u32 num_bitmap_blocks = (sb.nblocks + BITS_PER_BLOCK -1) / BITS_PER_BLOCK;
u32 num_bitmap_bytes = (sb.nblocks + 7) / 8;
u32 num_bitmap_bytes_unused = (BLOCK_SIZE - (num_bitmap_bytes%BLOCK_SIZE))%BLOCK_SIZE;

struct BitMap {
    u8 bits[num_bitmap_bytes];
    u8 unused[num_bitmap_bytes_unused] [[color(COLOR_UNUSED)]];
};

BitMap bitmap @ sb.bmapstart * BLOCK_SIZE;

#define VIMIXFS_N_DIRECT_BLOCKS 20

bitfield i_mode {
    S_IXOTH     : 1 [[comment("Others may execute.")]];
    S_IWOTH     : 1 [[comment("Others may write.")]];
    S_IROTH     : 1 [[comment("Others may read.")]];
    S_IXGRP     : 1 [[comment("Group members may execute.")]];
    S_IWGRP     : 1 [[comment("Group members may write.")]];
    S_IRGRP     : 1 [[comment("Group members may read.")]];
    S_IXUSR     : 1 [[comment("Owner may execute.")]];
    S_IWUSR     : 1 [[comment("Owner may write.")]];
    S_IRUSR     : 1 [[comment("Owner may read.")]];
    S_ISVTX     : 1 [[comment("Sticky bit.")]];
    S_ISGID     : 1 [[comment("Set GID.")]];
    S_ISUID     : 1 [[comment("Set UID.")]];
    S_IFIFO     : 1 [[comment("FIFO.")]];
    S_IFCHR     : 1 [[comment("Character device.")]];
    S_IFDIR     : 1 [[comment("Directory.")]];
    S_IFREG     : 1 [[comment("Regular file.")]];
    u16 reserved;
};

struct DataBlock {
    u8 data[BLOCK_SIZE];
};

#define VIMIXFS_NAME_MAX 60
struct Dirent
{
    u32 inum;
    char name[VIMIXFS_NAME_MAX];
};

struct dinode
{
    i_mode mode;
    u32 dev;
    u32 nlink;
    u32 size;
    u32 uid; 
    u32 gid; 
    type::time64_t ctime;
    type::time64_t mtime;
    u32 addrs[VIMIXFS_N_DIRECT_BLOCKS + 2];
    
    if (addrs[0] != 0)
    {
        if (mode.S_IFDIR == 1)
        {
            Dirent dir[BLOCK_SIZE/64] @ addrs[0] * BLOCK_SIZE;
        }
        else
        {
            DataBlock data @ addrs[0] * BLOCK_SIZE;
        }
    }

    if (addrs[1] != 0)
    {
        if (mode.S_IFDIR == 1)
        {
            Dirent dir[BLOCK_SIZE/64] @ addrs[1] * BLOCK_SIZE;
        }
        else
        {
            DataBlock data @ addrs[1] * BLOCK_SIZE;
        }
    }

    if (addrs[2] != 0)
    {
        if (mode.S_IFDIR == 1)
        {
            Dirent dir[BLOCK_SIZE/64] @ addrs[2] * BLOCK_SIZE;
        }
        else
        {
            DataBlock data @ addrs[2] * BLOCK_SIZE;
        }
    }

    if (addrs[3] != 0)
    {
        if (mode.S_IFDIR == 1)
        {
            Dirent dir[BLOCK_SIZE/64] @ addrs[3] * BLOCK_SIZE;
        }
        else
        {
            DataBlock data @ addrs[3] * BLOCK_SIZE;
        }
    }
    
};

#define DINODES_PER_BLOCK 4

struct Inodes {
    dinode disk_inodes[sb.ninode_blocks * DINODES_PER_BLOCK];
};

Inodes inodes @ sb.inodestart * BLOCK_SIZE;
