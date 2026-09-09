/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>
#include <vimixutils/file.h>

#if defined(BUILD_ON_HOST)
#include <linux/limits.h>
#else
#include <kernel/limits.h>
#endif

#define COPY_BUFFER_SIZE 2048
#define TEMP_FILE_ATTEMPTS 100

static int copy_data(int src_fd, const char *src, int dst_fd, const char *dst)
{
    char buffer[COPY_BUFFER_SIZE];
    while (true)
    {
        ssize_t bytes_read = read(src_fd, buffer, sizeof(buffer));
        if (bytes_read < 0)
        {
            fprintf(stderr, "cannot read %s: %s\n", src, strerror(errno));
            return 1;
        }
        if (bytes_read == 0) break;

        ssize_t written = 0;
        while (written < bytes_read)
        {
            ssize_t result =
                write(dst_fd, buffer + written, bytes_read - written);
            if (result <= 0)
            {
                int write_error = result < 0 ? errno : EIO;
                fprintf(stderr, "cannot write %s: %s\n", dst,
                        strerror(write_error));
                return 1;
            }
            written += result;
        }
    }

    return 0;
}

static int open_source(const char *src, struct stat *src_stat)
{
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0)
    {
        fprintf(stderr, "cannot open %s: %s\n", src, strerror(errno));
        return -1;
    }

    if (fstat(src_fd, src_stat) < 0)
    {
        fprintf(stderr, "cannot stat %s: %s\n", src, strerror(errno));
        close(src_fd);
        return -1;
    }
    if (!S_ISREG(src_stat->st_mode))
    {
        fprintf(stderr, "cannot copy %s: not a regular file\n", src);
        close(src_fd);
        return -1;
    }

    return src_fd;
}

static inline void copy_file_close_fds(int fd1, const char *name1, int fd2,
                                       const char *name2)
{
    if (close(fd1) < 0)
    {
        fprintf(stderr, "cannot close %s: %s\n", name1, strerror(errno));
    }
    if (close(fd2) < 0)
    {
        fprintf(stderr, "cannot close %s: %s\n", name2, strerror(errno));
    }
}

int copy_file(const char *src, const char *dst)
{
    struct stat src_stat, dst_stat;
    int src_fd = open_source(src, &src_stat);
    if (src_fd < 0) return 1;

    bool dst_created = false;
    int dst_fd = open(dst, O_WRONLY);
    if ((dst_fd < 0) && (errno == ENOENT))
    {
        dst_fd = open(dst, O_WRONLY | O_CREAT | O_EXCL, src_stat.st_mode);
        dst_created = dst_fd >= 0;
        if (dst_fd < 0 && errno == EEXIST) dst_fd = open(dst, O_WRONLY);
    }
    if (dst_fd < 0)
    {
        fprintf(stderr, "cannot open %s: %s\n", dst, strerror(errno));
        close(src_fd);
        return 1;
    }

    int result = 1;
    if (fstat(dst_fd, &dst_stat) < 0)
    {
        fprintf(stderr, "cannot stat %s: %s\n", dst, strerror(errno));
        copy_file_close_fds(dst_fd, dst, src_fd, src);
        if (dst_created) unlink(dst);
        return 1;
    }
    if ((src_stat.st_dev == dst_stat.st_dev) &&
        (src_stat.st_ino == dst_stat.st_ino))
    {
        fprintf(stderr, "cannot copy file to itself.\n");
        copy_file_close_fds(dst_fd, dst, src_fd, src);
        if (dst_created) unlink(dst);
        return 1;
    }
    if (S_ISREG(dst_stat.st_mode) && ftruncate(dst_fd, 0) < 0)
    {
        fprintf(stderr, "cannot truncate %s: %s\n", dst, strerror(errno));
        copy_file_close_fds(dst_fd, dst, src_fd, src);
        if (dst_created) unlink(dst);
        return 1;
    }

    result = copy_data(src_fd, src, dst_fd, dst);

    copy_file_close_fds(dst_fd, dst, src_fd, src);
    if (result != 0 && dst_created) unlink(dst);
    return result;
}

static int open_temporary(const char *dst, char *temporary,
                          size_t temporary_size, mode_t mode)
{
    const char *last_slash = strrchr(dst, '/');
    size_t prefix_len = last_slash == NULL ? 0 : (size_t)(last_slash - dst + 1);
    if (prefix_len >= temporary_size)
    {
        fprintf(stderr,
                "cannot create temporary destination: path is too long\n");
        return -1;
    }

    memcpy(temporary, dst, prefix_len);
    size_t suffix_size = temporary_size - prefix_len;

    for (unsigned int attempt = 0; attempt < TEMP_FILE_ATTEMPTS; attempt++)
    {
        int suffix_len = snprintf(temporary + prefix_len, suffix_size,
                                  ".mv-%ld-%u", (long)getpid(), attempt);
        if (suffix_len < 0 || (size_t)suffix_len >= suffix_size)
        {
            fprintf(stderr,
                    "cannot create temporary destination: path is too long\n");
            return -1;
        }

        int fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, mode);
        if (fd >= 0) return fd;
        if (errno != EEXIST)
        {
            fprintf(stderr, "cannot create %s: %s\n", temporary,
                    strerror(errno));
            return -1;
        }
    }

    fprintf(stderr, "cannot create a unique temporary destination\n");
    return -1;
}

int move_file_across_filesystems(const char *src, const char *dst)
{
    struct stat src_stat;
    int src_fd = open_source(src, &src_stat);
    if (src_fd < 0) return 1;

    char temporary[PATH_MAX];
    int dst_fd =
        open_temporary(dst, temporary, sizeof(temporary), src_stat.st_mode);
    if (dst_fd < 0)
    {
        close(src_fd);
        return 1;
    }

    int result = copy_data(src_fd, src, dst_fd, temporary);
    if ((result == 0) && (geteuid() == 0) &&
        (fchown(dst_fd, src_stat.st_uid, src_stat.st_gid) < 0))
    {
        fprintf(stderr, "cannot set owner of %s: %s\n", temporary,
                strerror(errno));
        result = 1;
    }
    if (result == 0 && fchmod(dst_fd, src_stat.st_mode) < 0)
    {
        fprintf(stderr, "cannot set mode of %s: %s\n", temporary,
                strerror(errno));
        result = 1;
    }

    if (close(dst_fd) < 0)
    {
        fprintf(stderr, "cannot close %s: %s\n", temporary, strerror(errno));
        result = 1;
    }
    if (close(src_fd) < 0)
    {
        fprintf(stderr, "cannot close %s: %s\n", src, strerror(errno));
        result = 1;
    }

    struct utimbuf times = {src_stat.st_mtime, src_stat.st_mtime};
    if ((result == 0) && (utime(temporary, &times) < 0))
    {
        fprintf(stderr, "cannot set time of %s: %s\n", temporary,
                strerror(errno));
        result = 1;
    }

    if (result == 0 && rename(temporary, dst) < 0)
    {
        fprintf(stderr, "cannot move temporary file to %s: %s\n", dst,
                strerror(errno));
        result = 1;
    }

    if (result != 0)
    {
        unlink(temporary);
        return 1;
    }

    if (unlink(src) < 0)
    {
        fprintf(stderr, "mv: cannot remove %s: %s\n", src, strerror(errno));
        return 1;
    }

    return 0;
}
