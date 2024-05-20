# xv6fs

Modified variant of the [file_system](file_system.md) found in xv6, which was designed after the original file system of UNIX version 6.


## Disk Layout

The disk is organized in blocks of `1024` bytes.

|                  | Boot | Super Block | Log Area | Inode Area | BMap Area | Data Blocks |
| ---------------- | ---- | ----------- | -------- | ---------- | --------- | ----------- |
| Blocks (example) | 1    | 1           | 30       | 13         | 1         | 2002        |


**Boot:**
The first block is reserved for the boot loader for historic reasons. This block is currently not used.

**Super Block:**
This block contains the struct `xv6fs_superblock` which describes the [file_system](file_system.md), including the position and size of the following areas. Everything in here stays static after the file system got created.

**Log Area:**
The first block contains the log header struct `logheader`. The following `LOGSIZE` blocks are used to store the data for blocks to be committed from the log. For each of the `LOGSIZE` blocks the header stores the destination block address.

See [xv6fs_log](xv6fs_log.md).

**Inode Area:**
The [inodes](inode.md) are stored sequentially on disk in an array as `struct xv6fs_dinode` (disk inode). The inode number is the index into this array. The amount of blocks reserved for this inode array defines the maximum of supported inodes and thus files and directories. 

Inode 1 is by definition the root directory of the file system.

[Directories](directory.md) are stored as files containing an array of `struct xv6fs_dirent`.
[Files](file.md) are stored in blocks in the data blocks area. The disk inode contains an array of block addresses for the files content. An additional block can be allocated for block addresses of larger files.

**BMap Area:**
Stores one bit per block as a use/free flag. The size of this area is defined by the size of the file system. Only blocks from the data blocks can be free, all blocks containing file system meta data (including this bitmap) are indicated as used.

**Data Blocks:**
Unstructured data blocks with the contents of the files and directories. Size of this area defined the usable disk size of the file system.


## Limits

**Max files:**
File and directory count are limited by the inode count. This is defined at file system creation in `mkfs` (see `MAX_ACTIVE_INODESS`). As [directories](directory.md) references inodes with a 16-bit unsigned integer, the maximal number of inodes in xv6fs is 65,535 (0 is a reserved value).

**Max files per directory;**
A `struct xv6fs_dirent` is `16` bytes long, thus 64 directory entries can be stored in one block. A file can be up to `12+256` blocks large. This limits the number of directory entries per directory to `17152` entries. Each dir contains `.` and `..`, so `17150` files or sub directories can be stored in one xv6fs directory.

**Max file size:**
Currently a disk inode can reference `12` data blocks. One additional block can be used as a second array of `1024/sizeof(uint32_t)` = `256` block addresses. The maximal file size supported is thus `(12+256)*1024` = 268 kb.

**Max file system size:**
Blocks are indexed by 32-bit unsigned integers. Thus file systems of up to 4 TB should be possible. However, xv6fs would still only support a maximum of 65,535 files with 268 kb each. So a 16 GB disk might be the practical limit.

**Max file name:**
Just 14 chars, limited by `XV6_NAME_MAX` and indirectly by the size of one `struct xv6fs_dirent`.


## Changes compared to xv6

- refactored and renamed some defines, moved code to separate generic and xv6fs specific code.
- split device disk inodes into char and block devices to correctly store different types of [devices](../devices/devices.md).


## Related

Video about the original xv6 file system: https://www.youtube.com/watch?v=o9sYiWj1F28&ab_channel=hhp3

---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [xv6fs](xv6fs.md) | [xv6fs_log](xv6fs_log.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
