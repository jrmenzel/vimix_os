/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/vimixfs.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

/// @brief Represents an opened VIMIX file system in memory.
struct vimixfs
{
    int fd;

    // super block of opened file system:
    struct vimixfs_superblock super_block;

    // log header of the opened file system:
    struct vimixfs_log_header log_header;

    // bitmap of used blocks from the file:
    uint8_t *bitmap;

    // to build a bitmap of used blocks and compare it to the bitmap of the fs
    // later:
    uint8_t *shadow_bitmap;
    size_t bitmap_errors;

    // store for each inode if it is in use and if it is referenced by a
    // directory:
#define INODE_UNUSED 0
#define INODE_REFERENCED 1
#define INODE_DEFINE 2
#define INODE_OK(x) \
    ((x == INODE_UNUSED) || (x == (INODE_REFERENCED & INODE_DEFINE)))
    uint8_t *inodes;
    size_t inode_errors;
};

#define INVALID_BLOCK_INDEX 0xFFFFFFFF

struct vimixfs_copy_params
{
    const char *src_path_on_host;
    const char *dst_dir_on_fs;
    uid_t uid;
    gid_t gid;
    mode_t fmode;
    mode_t dmode;
};

/// @brief Create an empty filesystem (just "." and "..")
/// @param vifs Externally allocated vimixfs struct
/// @param filename Name of file to create the filesystem in
/// @param fs_size_in_blocks New file system size in blocks
/// @return True on success, false on error
bool vimixfs_create(struct vimixfs *vifs, const char *filename,
                    size_t fs_size_in_blocks);

/// @brief Open an existing VIMIX file system
/// @param vifs Externally allocated vimixfs struct
/// @param filename Name of existing VIMIX file system
/// @return True on success, false on error
bool vimixfs_open(struct vimixfs *vifs, const char *filename);

/// @brief Close opened VIMIX file system and free resources.
/// @param vifs The opened VIMIX file system
void vimixfs_close(struct vimixfs *vifs);

/// @brief Gets the number of free blocks in the file system available for files
/// and dirs.
/// @param vifs The opened VIMIX file system
/// @return Number of free blocks
uint32_t vimixfs_get_free_block_count(struct vimixfs *vifs);

/// @brief Copy a directory of the VIMIX file system to a directory on the host
/// file system.
/// @param vifs The opened VIMIX file system
/// @param dir_inode_on_fs VIMIX FS inode of directory to copy
/// @param sub_path Sub path inside the VIMIX file system (for recursion)
/// @param dir_on_host Target dir on host file system
/// @return True on success, false on error
bool vimixfs_cp_dir_to_host(struct vimixfs *vifs, int32_t dir_inode_on_fs,
                            const char *sub_path, const char *dir_on_host);

/// @brief Copy a regular file from the VIMIX file system to the host file
/// system.
/// @param vifs The opened VIMIX file system
/// @param inode Inode of the VIMIX file to copy
/// @param filename Host filename to copy the file to
/// @return True on success, false on error
bool vimixfs_cp_file_to_host(struct vimixfs *vifs, int32_t inode,
                             const char *filename);

/// @brief Copies a file or directory from the host file system to the VIMIX
/// file system.
/// @param vifs The opened VIMIX file system
/// @param path_on_host Path to a file or directory on the host file system
/// @param path_in_fs Target path in the VIMIX file system.
/// @param param Copy parameters like user ID, group ID, file mode, etc.
/// @return True on success, false on error
bool vimixfs_cp_from_host(struct vimixfs *vifs, const char *path_on_host,
                          const char *path_in_fs,
                          struct vimixfs_copy_params *param);

/// @brief Copy a file from the host file system into the VIMIX file system.
/// @param vifs The opened VIMIX file system
/// @param path_on_host File to copy.
/// @param new_name File name in the VIMIX file system.
/// @param dir_inode_on_fs Parent dir to copy the file into.
/// @param param Copy parameters like user ID, group ID, file mode, etc.
/// @return True on success, false on error
bool vimixfs_cp_file_from_host(struct vimixfs *vifs, const char *path_on_host,
                               const char *new_name, int32_t dir_inode_on_fs,
                               struct vimixfs_copy_params *param);

/// @brief Recursively copy a directory from the host file system into the VIMIX
/// file system.
/// @param vifs The opened VIMIX file system
/// @param dir_on_host Directory on the host file system to copy
/// @param dir_inode_on_fs Taget directory inode in the VIMIX file system to
/// copy into.
/// @param param Copy parameters like user ID, group ID, file mode, etc.
/// @return True on success, false on error
bool vimixfs_cp_dir_from_host(struct vimixfs *vifs, const char *dir_on_host,
                              int32_t dir_inode_on_fs,
                              struct vimixfs_copy_params *param);

/// @brief Change meta data (uid, gid, mode) of a file or directory in the VIMIX
/// file system.
/// @param vifs The opened VIMIX file system
/// @param name_on_fs Name of the file or directory in the VIMIX file system to
/// be changed. It its a directory and ends with '/', the directory is updated
/// recursively.
/// @param param Meta data parameters to set.
/// @return True on success, false on error
bool vimixfs_change_metadata(struct vimixfs *vifs, const char *name_in_fs,
                             struct vimixfs_copy_params *param);
