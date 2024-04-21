/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/sleeplock.h>
#include <kernel/stat.h>
#include <kernel/xv6fs.h>

/// in-memory copy of an inode
struct inode
{
    uint dev;               ///< Device number
    uint inum;              ///< Inode number
    int ref;                ///< Reference count
    struct sleeplock lock;  ///< protects everything below here
    int valid;              ///< inode has been read from disk?

    short type;  ///< copy of disk inode
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

void init_root_file_system(int);
int inode_dir_link(struct inode*, char*, uint);
struct inode* inode_dir_lookup(struct inode*, char*, uint*);
struct inode* inode_alloc(uint, short);
struct inode* inode_dup(struct inode*);
void inode_init();
void inode_lock(struct inode*);
void inode_put(struct inode*);
void inode_unlock(struct inode*);
void inode_unlock_put(struct inode*);
void inode_update(struct inode*);
int file_name_cmp(const char*, const char*);
struct inode* inode_from_path(char*);
struct inode* inode_of_parent_from_path(char*, char*);
int inode_read(struct inode*, int, uint64, uint, uint);
void inode_stat(struct inode*, struct stat*);
int inode_write(struct inode*, int, uint64, uint, uint);
void inode_trunc(struct inode*);
