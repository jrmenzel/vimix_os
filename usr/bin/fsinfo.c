/* SPDX-License-Identifier: MIT */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <kernel/major.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vimixutils/minmax.h>
#include <vimixutils/path.h>
#include <vimixutils/sysfs.h>

// skip . and ..
bool skip_dir_entry(const char *file_name)
{
    if (file_name[0] == '.')
    {
        if (file_name[1] == 0) return true;
        if (file_name[1] == '.' && file_name[2] == 0) return true;
    }

    return false;
}

void print_file_system(const char *path_name)
{
    printf("File system: %s\n", path_name);

    DIR *dir = opendir(path_name);
    if (dir == NULL)
    {
        fprintf(stderr, "fsinfo: cannot open directory %s: %s\n", path_name,
                strerror(errno));
        return;
    }

    char full_path[PATH_MAX];
    struct dirent *dir_entry = NULL;
    while ((dir_entry = readdir(dir)))
    {
        if (skip_dir_entry(dir_entry->d_name)) continue;

        build_full_path(full_path, path_name, dir_entry->d_name);
        size_t value = get_from_sysfs(full_path);
        if (strcmp(dir_entry->d_name, "dev") == 0)
        {
            // print as major,minor
            printf("  %s: (%d,%d)\n", dir_entry->d_name, MAJOR(value),
                   MINOR(value));
        }
        else
        {
            printf("  %s: %zu\n", dir_entry->d_name, value);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[])
{
    const char *path_name = "/sys/fs/";
    DIR *dir = opendir(path_name);
    if (dir == NULL)
    {
        fprintf(stderr, "fsinfo: cannot open directory %s: %s\n", path_name,
                strerror(errno));
        return -1;
    }

    char full_path[PATH_MAX];
    struct dirent *dir_entry = NULL;
    while ((dir_entry = readdir(dir)))
    {
        if (skip_dir_entry(dir_entry->d_name)) continue;

        build_full_path(full_path, path_name, dir_entry->d_name);
        print_file_system(full_path);
    }

    closedir(dir);

    return 0;
}
