/* SPDX-License-Identifier: MIT */
#pragma once

#include "libvimixfs.h"

#include <kernel/vimixfs.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

// low level functions, so far only used internally

void vimixfs_read_log_header(struct vimixfs* vifs);

/// @brief Write bitmap of used blocks to file. Called before closing a changed
/// file system.
/// @param vifs The opened VIMIX file system
void vimixfs_write_bitmap(struct vimixfs* vifs);

/// @brief Reads a sector from the file to the buffer
/// @param sec sector index
/// @param buf destination buffer
bool vimixfs_read_sector(struct vimixfs* vifs, uint32_t sec, void* buf);

/// @brief Write one block to the file at location sec.
/// @param sec sector index
/// @param buf the buffer to read from
bool vimixfs_write_sector(struct vimixfs* vifs, uint32_t sec, void* buf);

void vimixfs_read_dinode(struct vimixfs* vifs, ino_t inum,
                         struct vimixfs_dinode* ip);

void vimixfs_write_dinode(struct vimixfs* vifs, ino_t inum,
                          struct vimixfs_dinode* ip);

size_t vimixfs_read_inode(struct vimixfs* vifs, struct vimixfs_dinode* din,
                          void* buffer, size_t off, size_t size);

ino_t vimixfs_get_free_inode(struct vimixfs* vifs);

ino_t vimixfs_get_inode_from_path(struct vimixfs* vifs, const char* path);

uint32_t vimixfs_get_next_free_block(struct vimixfs* vifs);

/// @brief Allocates a new unique inode number and creates a disk inode.
/// @param st stats of the new inode
/// @return number of the new inode
uint32_t vimixfs_i_alloc(struct vimixfs* vifs, struct stat* st);

uint32_t vimixfs_create_directory(struct vimixfs* vifs, uint32_t inode_parent,
                                  const char* dir_name, struct stat* st);

void vimixfs_add_directory_entry(struct vimixfs* vifs, uint32_t inode_new_entry,
                                 uint32_t inode_dir, const char* filename);

void vimixfs_iappend(struct vimixfs* vifs, ino_t inum, void* xp, int n);

ssize_t vimixfs_inode_get_dirent(struct vimixfs* vifs, int32_t inode_dir,
                                 struct vimixfs_dirent* dir_entry,
                                 ssize_t seek_pos);

uint32_t vimixfs_get_block_index(struct vimixfs* vifs,
                                 struct vimixfs_dinode* din,
                                 uint32_t block_number);