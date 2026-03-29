/* SPDX-License-Identifier: MIT */

#include "libvimixfs.h"
#include "libvimixfs_internal.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vimixutils/minmax.h>
#include <vimixutils/path.h>

void fix_dir_size(struct vimixfs *vifs, int32_t inode)
{
    // fix size of dir inode: round up offset to BLOCK_SIZE
    struct vimixfs_dinode din;
    vimixfs_read_dinode(vifs, inode, &din);
    uint32_t off = din.size;
    if (off % BLOCK_SIZE != 0)
    {
        off = ((off / BLOCK_SIZE) + 1) * BLOCK_SIZE;
        din.size = off;
        vimixfs_write_dinode(vifs, inode, &din);
    }
}

bool is_dot_or_dotdot(const char *file_name)
{
    if (file_name[0] == '.')
    {
        if (file_name[1] == 0) return true;
        if (file_name[1] == '.' && file_name[2] == 0) return true;
    }

    return false;
}

int get_min_fs_size_in_blocks()
{
    // min fs
    // 1 boot block
    // 1 super block
    // 5 log blocks
    // 1 inode block -> 8 inodes, 7 free
    // 1 bitmap block -> 8192 blocks
    // data blocks:
    // 1 = / dir
    // 1 free block for one tiny file
    // total: 11 blocks;
    return 16;
}

int get_log_size(size_t fs_size_in_blocks)
{
    if (fs_size_in_blocks <= 32)
    {
        return 5;  // impractical, but just for fun
    }
    else if (fs_size_in_blocks <= 128)
    {
        return 16;
    }

    return 32;
}

void make_default_stat(struct stat *st, mode_t mode)
{
    memset(st, 0, sizeof(*st));
    st->st_mode = mode;
    st->st_nlink = 1;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_size = 0;
    st->st_mtime = st->st_ctime = time(NULL);
}

void vimixfs_read_log_header(struct vimixfs *vifs)
{
    char block_buffer[BLOCK_SIZE];
    bool read_ok =
        vimixfs_read_sector(vifs, vifs->super_block.logstart, block_buffer);
    if (!read_ok)
    {
        fprintf(stderr, "ERROR: could not read log header\n");
        exit(1);
    }
    memmove(&vifs->log_header, block_buffer, sizeof(vifs->log_header));
}

int get_inode_blocks(size_t fs_size_in_blocks)
{
    // bit less than 2% of fs for inodes
    size_t ninode_blocks = fs_size_in_blocks / 64;

    if (ninode_blocks < 4)
    {
        ninode_blocks++;  // a bit extra for tiny fs
    }

    // as inode number is uint16_t
    const size_t max_inode_blocks = 0x10000 / VIMIXFS_INODES_PER_BLOCK;
    if (ninode_blocks > max_inode_blocks)
    {
        ninode_blocks = max_inode_blocks;
    }
    return ninode_blocks;
}

void set_bits(uint8_t *bitmap, size_t nbits)
{
    size_t nbytes = (nbits + 7) / 8;
    memset(bitmap, 0xFF, nbytes);
    // clear unused bits in last byte
    size_t unused_bits = (nbytes * 8) - nbits;
    if (unused_bits > 0)
    {
        bitmap[nbytes - 1] =
            bitmap[nbytes - 1] & (uint8_t)(0xFF >> unused_bits);
    }
}

bool vimixfs_create(struct vimixfs *vifs, const char *filename,
                    size_t fs_size_in_blocks)
{
    memset(vifs, 0, sizeof(*vifs));
    if (filename == NULL)
    {
        fprintf(stderr, "ERROR: filename is NULL\n");
        return false;
    }
    if (fs_size_in_blocks < get_min_fs_size_in_blocks())
    {
        fprintf(stderr, "ERROR: min file system size is %d blocks\n",
                get_min_fs_size_in_blocks());
        return false;
    }

    int nlog = get_log_size(fs_size_in_blocks);
    int ninode_blocks = get_inode_blocks(fs_size_in_blocks);

    int nbitmap = VIMIXFS_BLOCKS_FOR_BITMAP(fs_size_in_blocks);
    vifs->bitmap = malloc(nbitmap * BLOCK_SIZE);
    if (vifs->bitmap == NULL)
    {
        fprintf(stderr, "ERROR: out of memory\n");
        return false;
    }
    memset(vifs->bitmap, 0, nbitmap * BLOCK_SIZE);

    // block 0 is reserved (for a boot block)
    // block 1 is super block
    int nmeta = 2 + nlog + ninode_blocks + nbitmap;
    set_bits(vifs->bitmap, nmeta);

    // open and/or create output file
    vifs->fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (vifs->fd < 0)
    {
        fprintf(stderr, "ERROR: could not open file %s (error: %s)\n", filename,
                strerror(errno));
        vimixfs_close(vifs);
        return false;
    }

    // 1 fs block = 1 disk sector
    int nblocks = fs_size_in_blocks - nmeta;

    vifs->super_block.magic = VIMIXFS_MAGIC;
    vifs->super_block.size = fs_size_in_blocks;
    vifs->super_block.nblocks = nblocks;
    vifs->super_block.ninode_blocks = ninode_blocks;
    vifs->super_block.nlog = nlog;
    vifs->super_block.logstart = 2;
    vifs->super_block.inodestart = 2 + nlog;
    vifs->super_block.bmapstart = 2 + nlog + ninode_blocks;

    char block_buffer[BLOCK_SIZE];
    memset(block_buffer, 0, sizeof(block_buffer));

    // fill file with zeroes
    for (int i = 0; i < fs_size_in_blocks; i++)
    {
        vimixfs_write_sector(vifs, i, block_buffer);
    }

    memmove(block_buffer, &vifs->super_block, sizeof(vifs->super_block));
    vimixfs_write_sector(vifs, VIMIXFS_SUPER_BLOCK_NUMBER, block_buffer);

    // root dir
    struct stat st;
    make_default_stat(&st, S_IFDIR | 0755);
    uint32_t inode = vimixfs_i_alloc(vifs, &st);
    if (inode != VIMIXFS_ROOT_INODE)
    {
        fprintf(stderr, "ERROR: root inode is not inode 1\n");
        vimixfs_close(vifs);
        return false;
    }

    vimixfs_add_directory_entry(vifs, VIMIXFS_ROOT_INODE, VIMIXFS_ROOT_INODE,
                                ".");
    vimixfs_add_directory_entry(vifs, VIMIXFS_ROOT_INODE, VIMIXFS_ROOT_INODE,
                                "..");

    printf("Created empty VIMIX FS file system %s (%zd blocks)\n", filename,
           fs_size_in_blocks);

    return true;
}

bool vimixfs_open(struct vimixfs *vifs, const char *filename)
{
    memset(vifs, 0, sizeof(*vifs));
    vifs->fd = open(filename, O_RDWR);
    if (vifs->fd < 0)
    {
        return false;
    }

    char block_buffer[BLOCK_SIZE];
    bool read_ok =
        vimixfs_read_sector(vifs, VIMIXFS_SUPER_BLOCK_NUMBER, block_buffer);
    if (!read_ok)
    {
        vimixfs_close(vifs);
        return false;
    }
    memmove(&vifs->super_block, block_buffer, sizeof(vifs->super_block));

    // read bitmap
    size_t bitmap_size_in_bytes =
        VIMIXFS_BLOCKS_FOR_BITMAP(vifs->super_block.size) * BLOCK_SIZE;

    vifs->bitmap = (uint8_t *)malloc(bitmap_size_in_bytes);
    if (vifs->bitmap == NULL)
    {
        vimixfs_close(vifs);
        return false;
    }

    for (size_t i = 0; i < VIMIXFS_BLOCKS_FOR_BITMAP(vifs->super_block.size);
         ++i)
    {
        vimixfs_read_sector(vifs, vifs->super_block.bmapstart + i,
                            vifs->bitmap + (i * BLOCK_SIZE));
    }

    vifs->inodes = (uint8_t *)malloc(vifs->super_block.ninode_blocks *
                                     VIMIXFS_INODES_PER_BLOCK);
    for (size_t i = 0;
         i < vifs->super_block.ninode_blocks * VIMIXFS_INODES_PER_BLOCK; ++i)
    {
        vifs->inodes[i] = INODE_UNUSED;
    }

    vimixfs_read_log_header(vifs);

    return true;
}

void vimixfs_close(struct vimixfs *vifs)
{
    vimixfs_write_bitmap(vifs);

    if (vifs->fd >= 0)
    {
        close(vifs->fd);
        vifs->fd = -1;
    }

    if (vifs->bitmap != NULL)
    {
        free(vifs->bitmap);
        vifs->bitmap = NULL;
    }

    if (vifs->inodes != NULL)
    {
        free(vifs->inodes);
        vifs->inodes = NULL;
    }

    if (vifs->shadow_bitmap != NULL)
    {
        free(vifs->shadow_bitmap);
        vifs->shadow_bitmap = NULL;
    }
}

uint32_t vimixfs_get_free_block_count(struct vimixfs *vifs)
{
    uint32_t free_blocks = 0;

    uint64_t *bitmap = (uint64_t *)vifs->bitmap;
    size_t total_int64 = vifs->super_block.size / (8 * sizeof(uint64_t));
    size_t remaining_bits =
        vifs->super_block.size - total_int64 * (8 * sizeof(uint64_t));

    for (size_t i = 0; i < total_int64; ++i)
    {
        uint64_t bm = bitmap[i];
        if (bm == 0)
        {
            free_blocks += 64;
            continue;
        }
        for (int b = 0; b < 64; ++b)
        {
            if ((bm & ((uint64_t)1 << b)) == 0)
            {
                free_blocks++;
            }
        }
    }

    if (remaining_bits > 0)
    {
        uint64_t bm = bitmap[total_int64];
        for (size_t b = 0; b < remaining_bits; ++b)
        {
            if ((bm & ((uint64_t)1 << b)) == 0)
            {
                free_blocks++;
            }
        }
    }
    return free_blocks;
}

bool vimixfs_cp_dir_to_host(struct vimixfs *vifs, int32_t dir_inode_on_fs,
                            const char *sub_path, const char *dir_on_host)
{
    if (sub_path == NULL || dir_on_host == NULL)
    {
        return false;
    }
    char full_path_on_host[PATH_MAX];
    build_full_path(full_path_on_host, dir_on_host, sub_path);

    int fd = open(full_path_on_host, O_RDWR);
    if (fd < 0)
    {
        mkdir(full_path_on_host, 0755);
    }
    else
    {
        close(fd);
    }

    struct vimixfs_dirent dir_entry;
    ssize_t next_entry = 0;
    while (true)
    {
        next_entry = vimixfs_inode_get_dirent(vifs, dir_inode_on_fs, &dir_entry,
                                              next_entry);
        if (next_entry < 0) break;

        if (is_dot_or_dotdot(dir_entry.name)) continue;

        struct vimixfs_dinode din;
        vimixfs_read_dinode(vifs, dir_entry.inum, &din);

        char full_file_path_on_host[PATH_MAX];
        build_full_path(full_file_path_on_host, full_path_on_host,
                        dir_entry.name);

        v_mode_t mode = din.mode;

        if (mode & S_IFDIR)
        {
            printf("create dir %s\n", full_file_path_on_host);

            char new_sub_path[PATH_MAX];
            build_full_path(new_sub_path, sub_path, dir_entry.name);

            vimixfs_cp_dir_to_host(vifs, dir_entry.inum, new_sub_path,
                                   dir_on_host);
        }
        else if (mode & S_IFREG)
        {
            printf("create file %s\n", full_file_path_on_host);
            vimixfs_cp_file_to_host(vifs, dir_entry.inum,
                                    full_file_path_on_host);
        }
        else
        {
            printf("ignore %s\n", full_file_path_on_host);
        }
    };

    return true;
}

bool vimixfs_cp_file_to_host(struct vimixfs *vifs, int32_t inode,
                             const char *filename)
{
    if (filename == NULL)
    {
        return false;
    }

    struct vimixfs_dinode din;
    vimixfs_read_dinode(vifs, inode, &din);

    v_mode_t mode = din.mode;
    if ((mode & S_IFREG) == 0)
    {
        return false;
    }

    size_t file_size = din.size;
    char *buffer = malloc(file_size);

    size_t read = vimixfs_read_inode(vifs, &din, buffer, 0, file_size);
    if (read != file_size)
    {
        return false;
    }

    int fd = open(filename, O_RDWR | O_CREAT, 0655);
    ssize_t w = write(fd, buffer, file_size);
    if (w != file_size)
    {
        printf("Error writing to file\n");
    }
    close(fd);

    free(buffer);
    return true;
}

bool vimixfs_cp_from_host(struct vimixfs *vifs, const char *path_on_host,
                          const char *path_in_fs,
                          struct vimixfs_copy_params *param)
{
    if (path_on_host == NULL || path_in_fs == NULL || param == NULL)
    {
        return false;
    }
    bool ok;
    struct stat st;
    stat(path_on_host, &st);
    if (S_ISDIR(st.st_mode))
    {
        ino_t ino = vimixfs_get_inode_from_path(vifs, param->dst_dir_on_fs);
        if (ino == INVALID_INODE)
        {
            fprintf(stderr, "ERROR: could not find dir %s in fs\n",
                    param->dst_dir_on_fs);
            return false;
        }
        ok = vimixfs_cp_dir_from_host(vifs, path_on_host, ino, param);
    }
    else
    {
        size_t path_len = strlen(path_in_fs);
        const char *last_slash = strrchr(path_in_fs, '/');
        char name_in_fs[VIMIXFS_NAME_MAX + 1];
        if (last_slash == NULL)
        {
            return false;
        }

        size_t name_len = path_len - (size_t)(last_slash - path_in_fs) - 1;
        if (name_len > VIMIXFS_NAME_MAX)
        {
            name_len = VIMIXFS_NAME_MAX;
        }
        strncpy(name_in_fs, last_slash + 1, name_len);
        name_in_fs[name_len] = '\0';

        char *path = malloc((size_t)(last_slash - path_in_fs) + 1);
        strncpy(path, path_in_fs, (size_t)(last_slash - path_in_fs));
        path[(size_t)(last_slash - path_in_fs)] = '\0';

        ino_t ino = vimixfs_get_inode_from_path(vifs, path);
        if (ino == INVALID_INODE)
        {
            fprintf(stderr, "ERROR: could not find dir %s in fs\n", path);
            return false;
        }

        ok = vimixfs_cp_file_from_host(vifs, path_on_host, name_in_fs, ino,
                                       param);
    }

    return ok;
}

bool vimixfs_cp_file_from_host(struct vimixfs *vifs, const char *path_on_host,
                               const char *new_name, int32_t dir_inode_on_fs,
                               struct vimixfs_copy_params *param)
{
    int fd = open(path_on_host, O_RDONLY);
    if (fd < 0) return false;

    size_t max_file_size = VIMIXFS_MAX_FILE_SIZE_BLOCKS * BLOCK_SIZE;
    struct stat st;
    if (fstat(fd, &st) < 0) return false;
    if (st.st_size > max_file_size)
    {
        fprintf(stderr, "\033[1;31m");  // font color to red
        fprintf(stderr, "error: can't copy file %s because it is too big.\n",
                path_on_host);
        fprintf(stderr, "File size: %zd, max file size: %zd\n", st.st_size,
                max_file_size);
        fprintf(stderr, "\033[0m");  // reset font color
        return false;                // skip file
    }
    uint32_t blocks_free = vimixfs_get_free_block_count(vifs);
    if (st.st_size > blocks_free * BLOCK_SIZE)
    {
        fprintf(stderr, "\033[1;31m");  // font color to red
        fprintf(stderr, "error: can't copy file %s because it is too big.\n",
                path_on_host);
        fprintf(stderr, "File size: %zd, space left: %d\n", st.st_size,
                blocks_free * BLOCK_SIZE);
        fprintf(stderr, "\033[0m");  // reset font color
        return false;                // skip file
    }

    st.st_uid = param->uid;
    st.st_gid = param->gid;
    st.st_mode = param->fmode;
    ino_t inum = vimixfs_i_alloc(vifs, &st);

    vimixfs_add_directory_entry(vifs, inum, dir_inode_on_fs, new_name);

    int cc;
    char block_buffer[BLOCK_SIZE];
    while ((cc = read(fd, block_buffer, sizeof(block_buffer))) > 0)
    {
        vimixfs_iappend(vifs, inum, block_buffer, cc);
    }

    close(fd);

    fix_dir_size(vifs, dir_inode_on_fs);
    return true;
}

bool vimixfs_cp_dir_from_host(struct vimixfs *vifs, const char *dir_on_host,
                              int32_t dir_inode_on_fs,
                              struct vimixfs_copy_params *param)
{
    DIR *dir = opendir(dir_on_host);
    if (dir == NULL)
    {
        return false;
    }

    bool all_ok = true;
    char full_path[PATH_MAX];
    struct dirent *dir_entry = NULL;
    while ((dir_entry = readdir(dir)))
    {
        if (is_dot_or_dotdot(dir_entry->d_name)) continue;

        build_full_path(full_path, dir_on_host, dir_entry->d_name);
        struct stat st;
        if (stat(full_path, &st) < 0) return false;

        if (S_ISDIR(st.st_mode))
        {
            st.st_uid = param->uid;
            st.st_gid = param->gid;
            st.st_mode = param->dmode;
            uint32_t new_dir = vimixfs_create_directory(vifs, dir_inode_on_fs,
                                                        dir_entry->d_name, &st);

            all_ok &= vimixfs_cp_dir_from_host(vifs, full_path, new_dir, param);
        }
        else if (S_ISREG(st.st_mode))
        {
            all_ok &= vimixfs_cp_file_from_host(
                vifs, full_path, dir_entry->d_name, dir_inode_on_fs, param);
        }
    }

    closedir(dir);

    return all_ok;
}

bool vimixfs_change_metadata_of_dinode(struct vimixfs *vifs,
                                       struct vimixfs_dinode *din,
                                       struct vimixfs_copy_params *param)
{
    din->uid = param->uid;
    din->gid = param->gid;
    if (S_ISDIR(din->mode))
    {
        din->mode = param->dmode;
    }
    else
    {
        din->mode = param->fmode;
    }

    return true;
}

bool vimixfs_change_metadata_of_inode(struct vimixfs *vifs, ino_t inode,
                                      struct vimixfs_copy_params *param,
                                      bool recursive)
{
    struct vimixfs_dinode din;
    vimixfs_read_dinode(vifs, inode, &din);
    vimixfs_change_metadata_of_dinode(vifs, &din, param);
    vimixfs_write_dinode(vifs, inode, &din);

    if (recursive && S_ISDIR(din.mode))
    {
        struct vimixfs_dirent dir_entry;
        ssize_t next_entry = 0;
        while (true)
        {
            next_entry =
                vimixfs_inode_get_dirent(vifs, inode, &dir_entry, next_entry);
            if (next_entry < 0) break;

            if (is_dot_or_dotdot(dir_entry.name)) continue;

            vimixfs_change_metadata_of_inode(vifs, dir_entry.inum, param,
                                             recursive);
        };
    }

    return true;
}

bool vimixfs_change_metadata(struct vimixfs *vifs, const char *name_in_fs,
                             struct vimixfs_copy_params *param)
{
    if (name_in_fs == NULL || param == NULL)
    {
        return false;
    }

    size_t path_len = strlen(name_in_fs);
    char *path_copy = malloc(path_len + 1);
    strncpy(path_copy, name_in_fs, path_len + 1);
    path_copy[path_len] = '\0';

    bool recursive = false;
    if (path_len > 1 && name_in_fs[path_len - 1] == '/')
    {
        recursive = true;
        // remove '/' from path
        path_copy[path_len - 1] = '\0';
    }

    ino_t inode = vimixfs_get_inode_from_path(vifs, path_copy);
    if (inode == INVALID_INODE)
    {
        return false;
    }

    vimixfs_change_metadata_of_inode(vifs, inode, param, recursive);

    return true;
}

bool vimixfs_read_sector(struct vimixfs *vifs, uint32_t sec, void *buf)
{
    if ((sec != VIMIXFS_SUPER_BLOCK_NUMBER) && (sec >= vifs->super_block.size))
    {
        printf("vimixfs_read_sector: out of range\n");
        return false;
    }
    if (lseek(vifs->fd, sec * BLOCK_SIZE, SEEK_SET) != sec * BLOCK_SIZE)
    {
        printf("error: %s\n", strerror(errno));
        return false;
    }
    ssize_t bytes_read = read(vifs->fd, buf, BLOCK_SIZE);
    if (bytes_read == 0)
    {
        printf("error: unexpected end of file\n");
        return false;
    }
    if (bytes_read != BLOCK_SIZE)
    {
        printf("error: %s\n", strerror(errno));
        return false;
    }
    return true;
}

bool vimixfs_write_sector(struct vimixfs *vifs, uint32_t sec, void *buf)
{
    if (lseek(vifs->fd, sec * BLOCK_SIZE, SEEK_SET) < 0)
    {
        fprintf(stderr, "ERROR: seek failed in write_sector(), %s\n",
                strerror(errno));
        return false;
    }
    ssize_t written = write(vifs->fd, buf, BLOCK_SIZE);
    if (written != BLOCK_SIZE)
    {
        fprintf(stderr, "ERROR: write failed in write_sector(), %s\n",
                strerror(errno));
        return false;
    }
    return true;
}

void vimixfs_read_dinode(struct vimixfs *vifs, ino_t inum,
                         struct vimixfs_dinode *ip)
{
    char buf[BLOCK_SIZE];
    struct vimixfs_dinode *dip;

    uint32_t block_index = VIMIXFS_BLOCK_OF_INODE(inum, vifs->super_block);
    vimixfs_read_sector(vifs, block_index, buf);
    dip = ((struct vimixfs_dinode *)buf) + VIMIXFS_DINODE_OFFSET_IN_BLOCK(inum);
    *ip = *dip;  // copy struct into ip
}

void vimixfs_write_dinode(struct vimixfs *vifs, ino_t inum,
                          struct vimixfs_dinode *ip)
{
    char buf[BLOCK_SIZE];
    struct vimixfs_dinode *dip;

    uint32_t block_index = VIMIXFS_BLOCK_OF_INODE(inum, vifs->super_block);
    vimixfs_read_sector(vifs, block_index, buf);
    dip = ((struct vimixfs_dinode *)buf) + VIMIXFS_DINODE_OFFSET_IN_BLOCK(inum);
    *dip = *ip;  // copy struct of the inode to dip (== copy into buf)
    vimixfs_write_sector(vifs, block_index, buf);
}

uint32_t vimixfs_get_next_free_block(struct vimixfs *vifs)
{
    uint64_t *bitmap = (uint64_t *)vifs->bitmap;
    size_t bitmap_blocks = VIMIXFS_BLOCKS_FOR_BITMAP(vifs->super_block.size);
    size_t total_int64 = bitmap_blocks * BLOCK_SIZE / sizeof(uint64_t);
    for (size_t i = 0; i < total_int64; ++i)
    {
        uint64_t bm = bitmap[i];
        for (int b = 0; b < 64; ++b)
        {
            if ((bm & ((uint64_t)1 << b)) == 0)
            {
                // mark block as used
                bitmap[i] = bitmap[i] | ((uint64_t)1 << b);
                uint32_t block_number = (uint32_t)(i * 64 + b);
                return block_number;
            }
        }
    }
    return 0;
}

ino_t vimixfs_get_free_inode(struct vimixfs *vifs)
{
    struct vimixfs_superblock *sb = &vifs->super_block;
    uint8_t buffer[BLOCK_SIZE];

    size_t inode_end = sb->inodestart + sb->ninode_blocks;
    for (size_t block = sb->inodestart; block < inode_end; ++block)
    {
        vimixfs_read_sector(vifs, block, buffer);
        struct vimixfs_dinode *dinode = (struct vimixfs_dinode *)buffer;
        for (size_t i = 0; i < VIMIXFS_INODES_PER_BLOCK; ++i)
        {
            if (dinode[i].mode == VIMIXFS_INVALID_MODE)
            {
                ino_t inum =
                    VIMIXFS_INUM_OF_DINODE((block - sb->inodestart), i);
                if (inum == 0)
                {
                    continue;  // inode 0 is not used
                }
                return inum;
            }
        }
    }

    return INVALID_INODE;  // no more free inodes
}

void vimixfs_write_bitmap(struct vimixfs *vifs)
{
    size_t bitmap_blocks = VIMIXFS_BLOCKS_FOR_BITMAP(vifs->super_block.size);

    for (size_t b = 0; b < bitmap_blocks; ++b)
    {
        void *buf = vifs->bitmap + (b * BLOCK_SIZE);
        vimixfs_write_sector(vifs, vifs->super_block.bmapstart + b, buf);
    }
}

ino_t vimixfs_get_inode_from_path(struct vimixfs *vifs, const char *path)
{
    if (strcmp(path, "/") == 0)
    {
        return VIMIXFS_ROOT_INODE;
    }

    char path_copy[1024];
    strncpy(path_copy, path, sizeof(path_copy));
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *token;
    char *rest = path_copy;
    ino_t current_inode = VIMIXFS_ROOT_INODE;

    while ((token = strtok_r(rest, "/", &rest)))
    {
        struct vimixfs_dinode dinode;
        vimixfs_read_dinode(vifs, current_inode, &dinode);
        if (!S_ISDIR(dinode.mode))
        {
            return INVALID_INODE;  // not a directory
        }

        bool found = false;
        // search in directory entries
        size_t dir_size = dinode.size;
        size_t offset = 0;
        while (offset < dir_size)
        {
            struct vimixfs_dirent dirent;
            size_t read_bytes = vimixfs_read_inode(vifs, &dinode, &dirent,
                                                   offset, sizeof(dirent));
            if (read_bytes != sizeof(dirent))
            {
                return INVALID_INODE;  // read error
            }
            if (strncmp(dirent.name, token, VIMIXFS_NAME_MAX) == 0)
            {
                current_inode = dirent.inum;
                found = true;
                break;
            }
            offset += sizeof(dirent);
        }
        if (!found)
        {
            return INVALID_INODE;  // not found
        }
    }

    return current_inode;
}

size_t vimixfs_read_inode(struct vimixfs *vifs, struct vimixfs_dinode *din,
                          void *buffer, size_t off, size_t size)
{
    size_t inode_size = din->size;
    if (off > inode_size || off + size < off)
    {
        return 0;
    }
    if (off + size > inode_size)
    {
        size = inode_size - off;
    }

    uint32_t indirect[VIMIXFS_N_INDIRECT_BLOCKS];
    memset(indirect, 0, sizeof(indirect));
    size_t sector_of_indirect_blocks = din->addrs[VIMIXFS_INDIRECT_BLOCK_IDX];
    if (sector_of_indirect_blocks != 0)
    {
        vimixfs_read_sector(vifs, sector_of_indirect_blocks, (void *)indirect);
    }

    char buf[BLOCK_SIZE];
    size_t buffer_offset = 0;
    size_t read_total = 0;
    while (size > 0)
    {
        size_t block = off / BLOCK_SIZE;
        size_t off_in_block = off % BLOCK_SIZE;
        size_t read_from_sector = BLOCK_SIZE - off_in_block;
        if (read_from_sector > size) read_from_sector = size;
        uint32_t sector = 0;
        if (block < VIMIXFS_N_DIRECT_BLOCKS)
        {
            sector = din->addrs[block];
        }

        block -= VIMIXFS_N_DIRECT_BLOCKS;
        if ((sector == 0) && block < VIMIXFS_N_INDIRECT_BLOCKS)
        {
            sector = indirect[block];
        }
        block -= VIMIXFS_N_INDIRECT_BLOCKS;
        if ((sector == 0) &&
            (block < VIMIXFS_N_INDIRECT_BLOCKS * VIMIXFS_N_INDIRECT_BLOCKS))
        {
            // double indirect
            uint32_t double_indirect[VIMIXFS_N_INDIRECT_BLOCKS];
            memset(double_indirect, 0, sizeof(double_indirect));
            size_t sector_of_double_indirect_blocks =
                din->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX];
            if (sector_of_double_indirect_blocks != 0)
            {
                vimixfs_read_sector(vifs, sector_of_double_indirect_blocks,
                                    (void *)double_indirect);
                size_t idx1 = block / VIMIXFS_N_INDIRECT_BLOCKS;
                size_t idx2 = block % VIMIXFS_N_INDIRECT_BLOCKS;
                uint32_t next = double_indirect[idx1];
                if (next != 0)
                {
                    vimixfs_read_sector(vifs, next, (void *)indirect);
                    sector = indirect[idx2];
                }
            }
        }

        vimixfs_read_sector(vifs, sector, buf);
        memmove(buffer + buffer_offset, buf + off_in_block, read_from_sector);

        buffer_offset += read_from_sector;
        off += read_from_sector;
        size -= read_from_sector;
        read_total += read_from_sector;
    }

    return read_total;
}

void vimixfs_iappend(struct vimixfs *vifs, ino_t inum, void *xp, int n)
{
    struct vimixfs_dinode din;
    char buf[BLOCK_SIZE];

    vimixfs_read_dinode(vifs, inum, &din);
    uint32_t off = din.size;

    char *p = (char *)xp;
    if (p == NULL)
    {
        fprintf(stderr, "ERROR in iappend()\n");
        return;
    }

    while (n > 0)
    {
        uint32_t block_number = off / BLOCK_SIZE;
        uint32_t block_index =
            vimixfs_get_block_index(vifs, &din, block_number);
        if (block_index == INVALID_BLOCK_INDEX)
        {
            fprintf(stderr, "ERROR: no more free blocks\n");
            break;
        }

        uint32_t off_in_block = off % BLOCK_SIZE;
        uint32_t n1 = min(n, (int32_t)(BLOCK_SIZE - off_in_block));
        vimixfs_read_sector(vifs, block_index, buf);

        memmove(buf + off_in_block, p, n1);

        vimixfs_write_sector(vifs, block_index, buf);
        n -= n1;
        off += n1;
        p += n1;
    }
    din.size = off;
    vimixfs_write_dinode(vifs, inum, &din);
}

void vimixfs_add_directory_entry(struct vimixfs *vifs, uint32_t inode_new_entry,
                                 uint32_t inode_dir, const char *filename)
{
    struct vimixfs_dirent de;
    memset(&de, 0, sizeof(de));

    de.inum = inode_new_entry;
    strncpy(de.name, filename, VIMIXFS_NAME_MAX);
    vimixfs_iappend(vifs, inode_dir, &de, sizeof(de));
}

uint32_t vimixfs_create_directory(struct vimixfs *vifs, uint32_t inode_parent,
                                  const char *dir_name, struct stat *st)
{
    uint32_t inode = vimixfs_i_alloc(vifs, st);

    vimixfs_add_directory_entry(vifs, inode, inode, ".");
    vimixfs_add_directory_entry(vifs, inode_parent, inode, "..");
    vimixfs_add_directory_entry(vifs, inode, inode_parent, dir_name);

    return inode;
}

uint32_t vimixfs_i_alloc(struct vimixfs *vifs, struct stat *st)
{
    struct vimixfs_dinode din;

    memset(&din, 0, sizeof(din));

    din.mode = st->st_mode;
    din.nlink = 1;
    din.size = 0;
    din.uid = st->st_uid;
    din.gid = st->st_gid;
    din.ctime = st->st_ctime;
    din.mtime = st->st_mtime;

    ino_t inum = vimixfs_get_free_inode(vifs);
    vimixfs_write_dinode(vifs, inum, &din);
    return inum;
}

uint32_t vimixfs_get_block_index(struct vimixfs *vifs,
                                 struct vimixfs_dinode *din,
                                 uint32_t block_number)
{
    uint32_t indirect[VIMIXFS_N_INDIRECT_BLOCKS];

    assert(block_number < VIMIXFS_MAX_FILE_SIZE_BLOCKS);
    if (block_number < VIMIXFS_N_DIRECT_BLOCKS)
    {
        if (din->addrs[block_number] == 0)
        {
            din->addrs[block_number] = vimixfs_get_next_free_block(vifs);
        }
        return din->addrs[block_number];
    }

    block_number -= VIMIXFS_N_DIRECT_BLOCKS;
    if (block_number < VIMIXFS_N_INDIRECT_BLOCKS)
    {
        if (din->addrs[VIMIXFS_INDIRECT_BLOCK_IDX] == 0)
        {
            din->addrs[VIMIXFS_INDIRECT_BLOCK_IDX] =
                vimixfs_get_next_free_block(vifs);
        }
        vimixfs_read_sector(vifs, din->addrs[VIMIXFS_INDIRECT_BLOCK_IDX],
                            (char *)indirect);
        if (indirect[block_number] == 0)
        {
            indirect[block_number] = vimixfs_get_next_free_block(vifs);
            vimixfs_write_sector(vifs, din->addrs[VIMIXFS_INDIRECT_BLOCK_IDX],
                                 (char *)indirect);
        }
        return indirect[block_number];
    }

    block_number -= VIMIXFS_N_INDIRECT_BLOCKS;
    assert(block_number <
           VIMIXFS_N_INDIRECT_BLOCKS * VIMIXFS_N_INDIRECT_BLOCKS);

    if (din->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX] == 0)
    {
        din->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX] =
            vimixfs_get_next_free_block(vifs);
    }
    vimixfs_read_sector(vifs, din->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX],
                        (char *)indirect);
    uint32_t next_indirect_block =
        indirect[block_number / VIMIXFS_N_INDIRECT_BLOCKS];
    if (next_indirect_block == 0)
    {
        indirect[block_number / VIMIXFS_N_INDIRECT_BLOCKS] =
            vimixfs_get_next_free_block(vifs);
        next_indirect_block =
            indirect[block_number / VIMIXFS_N_INDIRECT_BLOCKS];
        vimixfs_write_sector(vifs,
                             din->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX],
                             (char *)indirect);
    }

    vimixfs_read_sector(vifs, next_indirect_block, (char *)indirect);
    if (indirect[block_number % VIMIXFS_N_INDIRECT_BLOCKS] == 0)
    {
        indirect[block_number % VIMIXFS_N_INDIRECT_BLOCKS] =
            vimixfs_get_next_free_block(vifs);
        vimixfs_write_sector(vifs, next_indirect_block, (char *)indirect);

        return indirect[block_number % VIMIXFS_N_INDIRECT_BLOCKS];
    }

    return 0;
}

ssize_t vimixfs_inode_get_dirent(struct vimixfs *vifs, int32_t inode_dir,
                                 struct vimixfs_dirent *dir_entry,
                                 ssize_t seek_pos)
{
    if (seek_pos < 0) return -1;

    struct vimixfs_dinode din;
    vimixfs_read_dinode(vifs, inode_dir, &din);

    v_mode_t mode = din.mode;
    if ((mode & S_IFDIR) == 0)
    {
        return -1;
    }

    ssize_t new_seek_pos = seek_pos;
    do
    {
        size_t read_bytes;
        read_bytes = vimixfs_read_inode(vifs, &din, (void *)dir_entry,
                                        (size_t)new_seek_pos,
                                        sizeof(struct vimixfs_dirent));
        if (read_bytes != sizeof(struct vimixfs_dirent))
        {
            return -1;
        }
        new_seek_pos += read_bytes;
    } while (dir_entry->inum == INVALID_INODE);  // skip unused entries

    return new_seek_pos;
}
