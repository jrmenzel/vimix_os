/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vimixutils/path.h>

#if defined(BUILD_ON_HOST)
#include <linux/limits.h>
#else
#include <kernel/limits.h>
#endif

const char *search_path[] = {"/usr/bin", "/usr/local/bin", NULL};

bool file_exists(const char *path)
{
    if (path == NULL) return false;
    struct stat st;
    if (stat(path, &st) < 0)
    {
        return false;
    }
    return true;
}

int build_full_path(char *dst, const char *path, const char *file)
{
    strncpy(dst, path, PATH_MAX);
    size_t len = strlen(dst);
    if (dst[len - 1] != '/')
    {
        dst[len] = '/';
        len++;
    }
    size_t name_len = strlen(file);
    if (len + name_len > PATH_MAX - 1)
    {
        return -1;
    }
    strncpy(dst + len, file, PATH_MAX - len);

    return 0;
}

char *find_program_in_path(const char *program)
{
    if (program == NULL) return NULL;

    // don't use the search path, e.g. for "./foo" or "/usr/bin/bar"
    if (program[0] == '.' || program[0] == '/')
    {
        bool exists = file_exists(program);
        if (exists)
        {
            char *res = (char *)malloc(strlen(program) + 1);
            if (res != NULL)
            {
                strcpy(res, program);
            }
            return res;
        }
        return NULL;
    }

    char *full_path = (char *)malloc(PATH_MAX);
    if (full_path == NULL) return NULL;

    for (size_t search_path_index = 0; search_path[search_path_index] != NULL;
         search_path_index++)
    {
        const char *current_search_path = search_path[search_path_index];
        build_full_path(full_path, current_search_path, program);

        if (file_exists(full_path))
        {
            return full_path;
        }
    }

    free(full_path);
    return NULL;
}

int build_copy_target(const char *src, const char *dst, char *new_dst,
                      size_t new_dst_size)
{
    struct stat dst_buf;
    int dst_stat_valid = stat(dst, &dst_buf);
    if (dst_stat_valid < 0 && errno != ENOENT)
    {
        fprintf(stderr, "cannot stat %s: %s\n", dst, strerror(errno));
        return 1;
    }

    size_t dst_len = strlen(dst);
    if (dst_stat_valid < 0 || !S_ISDIR(dst_buf.st_mode))
    {
        if (dst_len + 1 > new_dst_size)
        {
            fprintf(stderr, "destination path is too long\n");
            return 1;
        }
        memcpy(new_dst, dst, dst_len + 1);
        return 0;
    }

    const char *src_end = src + strlen(src);
    while (src_end > src + 1 && src_end[-1] == '/') src_end--;

    const char *file_name = src_end;
    while (file_name > src && file_name[-1] != '/') file_name--;

    size_t file_name_len = (size_t)(src_end - file_name);
    bool add_slash = dst_len == 0 || dst[dst_len - 1] != '/';
    if (file_name_len == 0 ||
        dst_len + add_slash + file_name_len + 1 > new_dst_size)
    {
        fprintf(stderr, "destination path is too long\n");
        return 1;
    }

    memcpy(new_dst, dst, dst_len);
    size_t offset = dst_len;
    if (add_slash) new_dst[offset++] = '/';
    memcpy(new_dst + offset, file_name, file_name_len);
    new_dst[offset + file_name_len] = '\0';
    return 0;
}
