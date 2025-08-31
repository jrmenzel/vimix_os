/* SPDX-License-Identifier: MIT */

#include <stddef.h>
#include <stdint.h>
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

const char* search_path[] = {"/usr/bin", "/usr/local/bin", NULL};

bool file_exists(const char* path)
{
    if (path == NULL) return false;
    struct stat st;
    if (stat(path, &st) < 0)
    {
        return false;
    }
    return true;
}

int build_full_path(char* dst, const char* path, const char* file)
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

char* find_program_in_path(const char* program)
{
    if (program == NULL) return NULL;

    // don't use the search path, e.g. for "./foo" or "/usr/bin/bar"
    if (program[0] == '.' || program[0] == '/')
    {
        bool exists = file_exists(program);
        if (exists)
        {
            char* res = (char*)malloc(strlen(program) + 1);
            if (res != NULL)
            {
                strcpy(res, program);
            }
            return res;
        }
        return NULL;
    }

    char* full_path = (char*)malloc(PATH_MAX);

    for (size_t search_path_index = 0; search_path[search_path_index] != NULL;
         search_path_index++)
    {
        const char* current_search_path = search_path[search_path_index];
        build_full_path(full_path, current_search_path, program);

        if (file_exists(full_path))
        {
            return full_path;
        }
    }

    free(full_path);
    return NULL;
}
