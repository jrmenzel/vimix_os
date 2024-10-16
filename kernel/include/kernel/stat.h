/* SPDX-License-Identifier: MIT */
/*
 * Some defines / values from Minix 2 and Linux
 */
#pragma once

#include <kernel/kernel.h>

/// @brief An inode number.
typedef long unsigned ino_t;

struct stat
{
    dev_t st_dev;       ///< File system's disk device
    ino_t st_ino;       ///< Inode number
    mode_t st_mode;     ///< File mode
    int16_t st_nlink;   ///< Number of links to file
    dev_t st_rdev;      ///< Device number if file is a char/block device
    size_t st_size;     ///< Size of file in bytes
    size_t st_blksize;  ///< Optimal block size for I/O
    size_t st_blocks;   ///< Number 512-byte blocks allocated
};

/// Traditional mask definitions for st_mode.
/// The ugly casts on only some of the definitions are to avoid suprising sign
/// extensions such as S_IFREG != (mode_t) S_IFREG when ints are 32 bits.
#define S_IFMT ((mode_t)0170000)   ///< type of file
#define S_IFREG ((mode_t)0100000)  //< regular
#define S_IFBLK 0060000            ///< block special
#define S_IFDIR 0040000            ///< directory
#define S_IFCHR 0020000            ///< character special
#define S_IFIFO 0010000            ///< this is a FIFO
#define S_ISUID 0004000            ///< set user id on execution
#define S_ISGID \
    0002000            ///< set group id on execution
                       ///< next is reserved for future use
#define S_ISVTX 01000  ///< save swapped text even after use

/// POSIX masks for st_mode.
#define S_IRWXU 00700  ///< owner:  rwx------
#define S_IRUSR 00400  ///< owner:  r--------
#define S_IWUSR 00200  ///< owner:  -w-------
#define S_IXUSR 00100  ///< owner:  --x------

#define S_IRWXG 00070  ///< group:  ---rwx---
#define S_IRGRP 00040  ///< group:  ---r-----
#define S_IWGRP 00020  ///< group:  ----w----
#define S_IXGRP 00010  ///< group:  -----x---

#define S_IRWXO 00007  ///< others: ------rwx
#define S_IROTH 00004  ///< others: ------r--
#define S_IWOTH 00002  ///< others: -------w-
#define S_IXOTH 00001  ///< others: --------x

/// The following macros test st_mode (from POSIX Sec. 5.6.1.1).
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)   ///< is a reg file
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)   ///< is a directory
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)   ///< is a char spec
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)   ///< is a block spec
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)  ///< is a pipe/FIFO

#define INODE_HAS_TYPE(i) \
    ((S_ISREG(i)) || (S_ISDIR(i)) || (S_ISCHR(i) || S_ISBLK(i) || S_ISFIFO(i)))

/// File types
///
/// NOTE! These match bits 12..15 of st_mode (see above)
/// (ie "(i_mode >> 12) & 15")
/// Used in struct dirent
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14
