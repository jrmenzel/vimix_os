/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <kernel/vimixfs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <vimixutils/minmax.h>
#include <vimixutils/path.h>

#if defined(BUILD_ON_HOST)
#include <linux/limits.h>
#else
#include <kernel/limits.h>
#endif

#include "libvimixfs.h"

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

struct vimixfs g_fs_file;

void write_sector(uint32_t, void *);
void write_dinode(ino_t, struct vimixfs_dinode *);
void read_dinode(ino_t inum, struct vimixfs_dinode *ip);
bool read_sector(uint32_t sec, void *buf);
uint32_t i_alloc(struct stat *st);
void iappend(ino_t inum, void *p, int n);
void die(const char *);

void add_directory_entry(uint32_t inode_new_entry, uint32_t inode_dir,
                         const char *filename)
{
    struct vimixfs_dirent de;
    memset(&de, 0, sizeof(de));

    de.inum = inode_new_entry;
    strncpy(de.name, filename, VIMIXFS_NAME_MAX);
    iappend(inode_dir, &de, sizeof(de));
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

uint32_t create_root_directory()
{
    struct stat st;
    make_default_stat(&st, S_IFDIR | 0755);
    uint32_t inode = i_alloc(&st);
    assert(inode == VIMIXFS_ROOT_INODE);

    add_directory_entry(VIMIXFS_ROOT_INODE, VIMIXFS_ROOT_INODE, ".");
    add_directory_entry(VIMIXFS_ROOT_INODE, VIMIXFS_ROOT_INODE, "..");

    return inode;
}

uint32_t create_directory(uint32_t inode_parent, const char *dir_name,
                          struct stat *st)
{
    uint32_t inode = i_alloc(st);

    add_directory_entry(inode, inode, ".");
    add_directory_entry(inode_parent, inode, "..");
    add_directory_entry(inode, inode_parent, dir_name);

    return inode;
}

/// @brief Create an empty filesystem (just "." and "..")
/// @param filename output filename of the file system
/// @return root inode number
uint32_t create_empty_filesystem(const char *filename, size_t fs_size)
{
    if (fs_size % BLOCK_SIZE != 0)
    {
        printf("ERROR: file system size must be a multiple of BLOCK_SITE %d\n",
               BLOCK_SIZE);
        exit(EXIT_FAILURE);
    }

    size_t fs_size_in_blocks = fs_size / BLOCK_SIZE;

    if (!vimixfs_create(&g_fs_file, filename, fs_size_in_blocks))
    {
        exit(EXIT_FAILURE);
    }

    uint32_t rootino = create_root_directory();
    return rootino;
}

void fix_dir_size(int32_t inode)
{
    // fix size of dir inode: round up offset to BLOCK_SIZE
    struct vimixfs_dinode din;
    read_dinode(inode, &din);
    uint32_t off = din.size;
    if (off % BLOCK_SIZE != 0)
    {
        off = ((off / BLOCK_SIZE) + 1) * BLOCK_SIZE;
        din.size = off;
        write_dinode(inode, &din);
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

bool copy_file_to_filesystem(const char *path_on_host, const char *new_name,
                             int32_t dir_inode_on_fs)
{
    int fd = open(path_on_host, O_RDONLY);
    if (fd < 0) die(path_on_host);

    size_t max_file_size = VIMIXFS_MAX_FILE_SIZE_BLOCKS * BLOCK_SIZE;
    struct stat st;
    if (fstat(fd, &st) < 0) return false;
    if (st.st_size > max_file_size)
    {
        printf("\033[1;31m");  // font color to red
        printf("error: can't copy file %s because it is too big.\n",
               path_on_host);
        printf("File size: %zd, max file size: %zd\n", st.st_size,
               max_file_size);
        printf("\033[0m");  // reset font color
        return false;       // skip file
    }
    uint32_t blocks_free = vimixfs_get_free_block_count(&g_fs_file);
    if (st.st_size > blocks_free * BLOCK_SIZE)
    {
        printf("\033[1;31m");  // font color to red
        printf("error: can't copy file %s because it is too big.\n",
               path_on_host);
        printf("File size: %zd, space left: %d\n", st.st_size,
               blocks_free * BLOCK_SIZE);
        printf("\033[0m");  // reset font color
        return false;       // skip file
    }

    ino_t inum = i_alloc(&st);

    add_directory_entry(inum, dir_inode_on_fs, new_name);

    int cc;
    char block_buffer[BLOCK_SIZE];
    while ((cc = read(fd, block_buffer, sizeof(block_buffer))) > 0)
    {
        iappend(inum, block_buffer, cc);
    }

    close(fd);

    fix_dir_size(dir_inode_on_fs);
    return true;
}

bool copy_dir_to_filesystem(const char *dir_on_host, int32_t dir_inode_on_fs)
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
            uint32_t new_dir =
                create_directory(dir_inode_on_fs, dir_entry->d_name, &st);

            all_ok &= copy_dir_to_filesystem(full_path, new_dir);
        }
        else if (S_ISREG(st.st_mode))
        {
            all_ok &= copy_file_to_filesystem(full_path, dir_entry->d_name,
                                              dir_inode_on_fs);
        }
    }

    closedir(dir);

    return all_ok;
}

int32_t open_filesystem(const char *filename)
{
    if (!vimixfs_open(&g_fs_file, filename))
    {
        die(filename);
    }

    return VIMIXFS_ROOT_INODE;
}

size_t read_inode(struct vimixfs_dinode *din, void *buffer, size_t off,
                  size_t size)
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
    size_t sector_of_indirect_blocks = din->addrs[VIMIXFS_INDIRECT_BLOCK_IDX];
    if (sector_of_indirect_blocks != 0)
    {
        read_sector(sector_of_indirect_blocks, (void *)indirect);
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
            size_t sector_of_double_indirect_blocks =
                din->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX];
            if (sector_of_double_indirect_blocks != 0)
            {
                read_sector(sector_of_double_indirect_blocks,
                            (void *)double_indirect);
                size_t idx1 = block / VIMIXFS_N_INDIRECT_BLOCKS;
                size_t idx2 = block % VIMIXFS_N_INDIRECT_BLOCKS;
                uint32_t next = double_indirect[idx1];
                if (next != 0)
                {
                    read_sector(next, (void *)indirect);
                    sector = indirect[idx2];
                }
            }
        }

        read_sector(sector, buf);
        memmove(buffer + buffer_offset, buf + off_in_block, read_from_sector);

        buffer_offset += read_from_sector;
        off += read_from_sector;
        size -= read_from_sector;
        read_total += read_from_sector;
    }

    return read_total;
}

ssize_t inode_get_dirent(int32_t inode_dir, struct vimixfs_dirent *dir_entry,
                         ssize_t seek_pos)
{
    if (seek_pos < 0) return -1;

    struct vimixfs_dinode din;
    read_dinode(inode_dir, &din);

    v_mode_t mode = din.mode;
    if ((mode & S_IFDIR) == 0)
    {
        return -1;
    }

    ssize_t new_seek_pos = seek_pos;
    do {
        size_t read_bytes;
        read_bytes = read_inode(&din, (void *)dir_entry, (size_t)new_seek_pos,
                                sizeof(struct vimixfs_dirent));
        if (read_bytes != sizeof(struct vimixfs_dirent))
        {
            return -1;
        }
        new_seek_pos += read_bytes;
    } while (dir_entry->inum == INVALID_INODE);  // skip unused entries

    return new_seek_pos;
}

void copy_out_file(int32_t inode, const char *filename)
{
    struct vimixfs_dinode din;
    read_dinode(inode, &din);

    v_mode_t mode = din.mode;
    if ((mode & S_IFREG) == 0)
    {
        return;
    }

    size_t file_size = din.size;
    char *buffer = malloc(file_size);

    size_t read = read_inode(&din, buffer, 0, file_size);
    if (read != file_size)
    {
        return;
    }

    int fd = open(filename, O_RDWR | O_CREAT, 0655);
    ssize_t w = write(fd, buffer, file_size);
    if (w != file_size)
    {
        printf("Error writing to file\n");
    }
    close(fd);

    free(buffer);
}

bool copy_filesystem_to_dir(int32_t dir_inode_on_fs, const char *sub_path,
                            const char *dir_on_host)
{
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
        next_entry = inode_get_dirent(dir_inode_on_fs, &dir_entry, next_entry);
        if (next_entry < 0) break;

        if (is_dot_or_dotdot(dir_entry.name)) continue;

        struct vimixfs_dinode din;
        read_dinode(dir_entry.inum, &din);

        char full_file_path_on_host[PATH_MAX];
        build_full_path(full_file_path_on_host, full_path_on_host,
                        dir_entry.name);

        v_mode_t mode = din.mode;

        if (mode & S_IFDIR)
        {
            printf("create dir %s\n", full_file_path_on_host);

            char new_sub_path[PATH_MAX];
            build_full_path(new_sub_path, sub_path, dir_entry.name);

            copy_filesystem_to_dir(dir_entry.inum, new_sub_path, dir_on_host);
        }
        else if (mode & S_IFREG)
        {
            printf("create file %s\n", full_file_path_on_host);
            copy_out_file(dir_entry.inum, full_file_path_on_host);
        }
        else
        {
            printf("ignore %s\n", full_file_path_on_host);
        }
    };

    return true;
}

int main(int argc, char *argv[])
{
    if (argc == 4)
    {
        uint32_t rootino;
        if (strcmp(argv[2], "--in") == 0)
        {
            // --in: create a new fs and copy in a directory
            size_t fs_size = 8 * 1024 * BLOCK_SIZE;
            rootino = create_empty_filesystem(argv[1], fs_size);
            bool ok = copy_dir_to_filesystem(argv[3], rootino);
            vimixfs_write_bitmap(&g_fs_file);
            vimixfs_close(&g_fs_file);
            return ok ? 0 : 1;
        }
        else if (strcmp(argv[2], "--out") == 0)
        {
            // --out: open an existing fs and copy out everything to a
            // directory
            rootino = open_filesystem(argv[1]);
            bool ok = copy_filesystem_to_dir(rootino, "/", argv[3]);
            vimixfs_close(&g_fs_file);
            return ok ? 0 : 1;
        }
        else if (strcmp(argv[2], "--create") == 0)
        {
            size_t fs_size = atoi(argv[3]);

            rootino = create_empty_filesystem(argv[1], fs_size * BLOCK_SIZE);
            vimixfs_write_bitmap(&g_fs_file);
            vimixfs_close(&g_fs_file);
            return 0;
        }
    }

    fprintf(stderr, "Usage: mkfs fs.img [--in|--out] <dir>\n");
    fprintf(stderr, "       mkfs fs.img --create <size in blocks/kb>\n");
    return 1;
}

/// @brief Write one block to the file at location sec.
/// @param sec sector index
/// @param buf the buffer to read from
void write_sector(uint32_t sec, void *buf)
{
    if (lseek(g_fs_file.fd, sec * BLOCK_SIZE, 0) != sec * BLOCK_SIZE)
        die("lseek");
    if (write(g_fs_file.fd, buf, BLOCK_SIZE) != BLOCK_SIZE) die("write");
}

void write_dinode(ino_t inum, struct vimixfs_dinode *ip)
{
    char buf[BLOCK_SIZE];
    struct vimixfs_dinode *dip;

    uint32_t block_index = VIMIXFS_BLOCK_OF_INODE(inum, g_fs_file.super_block);
    read_sector(block_index, buf);
    dip = ((struct vimixfs_dinode *)buf) + (inum % VIMIXFS_INODES_PER_BLOCK);
    *dip = *ip;  // copy struct of the inode to dip (== copy into buf)
    write_sector(block_index, buf);
}

void read_dinode(ino_t inum, struct vimixfs_dinode *ip)
{
    char buf[BLOCK_SIZE];
    struct vimixfs_dinode *dip;

    uint32_t block_index = VIMIXFS_BLOCK_OF_INODE(inum, g_fs_file.super_block);
    read_sector(block_index, buf);
    dip = ((struct vimixfs_dinode *)buf) + (inum % VIMIXFS_INODES_PER_BLOCK);
    *ip = *dip;  // copy struct into ip
}

/// @brief Reads a sector from the file to the buffer
/// @param sec sector index
/// @param buf destination buffer
bool read_sector(uint32_t sec, void *buf)
{
    if (sec >= g_fs_file.super_block.size)
    {
        printf("read_sector: out of range\n");
        return false;
    }
    if (lseek(g_fs_file.fd, sec * BLOCK_SIZE, 0) != sec * BLOCK_SIZE)
    {
        printf("error: %s\n", strerror(errno));
        die("lseek failed in read_sector()");
    }
    ssize_t bytes_read = read(g_fs_file.fd, buf, BLOCK_SIZE);
    if (bytes_read == 0)
    {
        printf("error: unexpected end of file\n");
        die("read failed in read_sector()");
    }
    if (bytes_read != BLOCK_SIZE)
    {
        printf("error: %s\n", strerror(errno));
        die("read failed in read_sector()");
    }
    return true;
}

/// @brief Allocates a new unique inode number and creates a disk inode.
/// @param st stats of the new inode
/// @return number of the new inode
uint32_t i_alloc(struct stat *st)
{
    ino_t inum = g_fs_file.freeinode++;
    struct vimixfs_dinode din;

    memset(&din, 0, sizeof(din));

    din.mode = st->st_mode;
    din.nlink = 1;
    din.size = 0;
    din.uid = st->st_uid;
    din.gid = st->st_gid;
    din.ctime = st->st_ctime;
    din.mtime = st->st_mtime;
    write_dinode(inum, &din);
    return inum;
}

uint32_t get_block_index(struct vimixfs_dinode *din, uint32_t block_number)
{
    uint32_t indirect[VIMIXFS_N_INDIRECT_BLOCKS];

    assert(block_number < VIMIXFS_MAX_FILE_SIZE_BLOCKS);
    if (block_number < VIMIXFS_N_DIRECT_BLOCKS)
    {
        if (din->addrs[block_number] == 0)
        {
            din->addrs[block_number] = get_next_free_block(&g_fs_file);
        }
        return din->addrs[block_number];
    }

    block_number -= VIMIXFS_N_DIRECT_BLOCKS;
    if (block_number < VIMIXFS_N_INDIRECT_BLOCKS)
    {
        if (din->addrs[VIMIXFS_INDIRECT_BLOCK_IDX] == 0)
        {
            din->addrs[VIMIXFS_INDIRECT_BLOCK_IDX] =
                get_next_free_block(&g_fs_file);
        }
        read_sector(din->addrs[VIMIXFS_INDIRECT_BLOCK_IDX], (char *)indirect);
        if (indirect[block_number] == 0)
        {
            indirect[block_number] = get_next_free_block(&g_fs_file);
            write_sector(din->addrs[VIMIXFS_INDIRECT_BLOCK_IDX],
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
            get_next_free_block(&g_fs_file);
    }
    read_sector(din->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX],
                (char *)indirect);
    uint32_t next_indirect_block =
        indirect[block_number / VIMIXFS_N_INDIRECT_BLOCKS];
    if (next_indirect_block == 0)
    {
        indirect[block_number / VIMIXFS_N_INDIRECT_BLOCKS] =
            get_next_free_block(&g_fs_file);
        next_indirect_block =
            indirect[block_number / VIMIXFS_N_INDIRECT_BLOCKS];
        write_sector(din->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX],
                     (char *)indirect);
    }

    read_sector(next_indirect_block, (char *)indirect);
    if (indirect[block_number % VIMIXFS_N_INDIRECT_BLOCKS] == 0)
    {
        indirect[block_number % VIMIXFS_N_INDIRECT_BLOCKS] =
            get_next_free_block(&g_fs_file);
        write_sector(next_indirect_block, (char *)indirect);

        return indirect[block_number % VIMIXFS_N_INDIRECT_BLOCKS];
    }

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
void iappend(ino_t inum, void *xp, int n)
{
    struct vimixfs_dinode din;
    char buf[BLOCK_SIZE];

    read_dinode(inum, &din);
    uint32_t off = din.size;
    // printf("append inum %d at off %d sz %d\n", inum, off, n);

    char *p = (char *)xp;
    if (p == NULL)
    {
        printf("ERROR in iappend()\n");
        return;
    }

    while (n > 0)
    {
        uint32_t block_number = off / BLOCK_SIZE;
        uint32_t block_index = get_block_index(&din, block_number);
        if (block_index == INVALID_BLOCK_INDEX)
        {
            fprintf(stderr, "ERROR: no more free blocks\n");
            break;
        }

        // uint32_t n1 = min(n, (block_number + 1) * BLOCK_SIZE - off);
        uint32_t n1 = min(n, BLOCK_SIZE);
        read_sector(block_index, buf);

        memmove(buf + off - (block_number * BLOCK_SIZE), p, n1);

        write_sector(block_index, buf);
        n -= n1;
        off += n1;
        p += n1;
    }
    din.size = off;
    write_dinode(inum, &din);
}
#pragma GCC diagnostic pop

/// @brief Exit the program after printing a error message
/// @param error_message the message
void die(const char *error_message)
{
    printf("ERROR: %s\n", error_message);
    exit(1);
}
