/* SPDX-License-Identifier: MIT */

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FLAG_RECURSIVE 1

struct path_list
{
    char **items;
    size_t count;
    size_t capacity;
};

void path_list_init(struct path_list *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void free_path_list(struct path_list *list)
{
    for (size_t i = 0; i < list->count; i++) free(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void path_list_add(struct path_list *list, const char *path)
{
    if (list->count == list->capacity)
    {
        // start with 16 entries, double the capacity each time we run out of
        // space
        size_t newcap = list->capacity ? list->capacity * 2 : 16;
        list->items = realloc(list->items, newcap * sizeof(char *));
        list->capacity = newcap;
    }
    list->items[list->count++] = strdup(path);
}

void collect_paths(const char *path, struct path_list *files,
                   struct path_list *dirs)
{
    struct stat st;
    if (stat(path, &st) < 0)
    {
        fprintf(stderr, "file or directory %s not found\n", path);
        return;
    }
    if (S_ISDIR(st.st_mode))
    {
        DIR *dir = opendir(path);
        if (!dir)
        {
            fprintf(stderr, "rm: failed to open directory %s: %s\n", path,
                    strerror(errno));
            return;
        }
        struct dirent *dir_entry = NULL;
        while ((dir_entry = readdir(dir)))
        {
            if ((dir_entry->d_name[0] == '.') &&
                ((dir_entry->d_name[1] == 0) ||
                 (dir_entry->d_name[1] == '.' && dir_entry->d_name[2] == 0)))
            {
                // skip . and ..
                continue;
            }
            char child[PATH_MAX];
            snprintf(child, PATH_MAX, "%s/%s", path, dir_entry->d_name);
            collect_paths(child, files, dirs);
        }
        closedir(dir);

        path_list_add(dirs, path);  // add directory after its contents
    }
    else
    {
        path_list_add(files, path);
    }
}

int rm(const char *path_name, int flags)
{
    if (flags & FLAG_RECURSIVE)
    {
        struct path_list files, dirs;
        path_list_init(&files);
        path_list_init(&dirs);

        collect_paths(path_name, &files, &dirs);
        // printf("Files to delete:\n");
        for (size_t i = 0; i < files.count; i++)
        {
            // printf("rm %s\n", files.items[i]);
            if (unlink(files.items[i]) < 0)
            {
                fprintf(stderr, "rm: failed to delete file %s: %s\n",
                        files.items[i], strerror(errno));
            }
        }
        // printf("Directories to delete:\n");
        for (size_t i = 0; i < dirs.count; i++)
        {
            // printf("rm %s\n", dirs.items[i]);
            if (rmdir(dirs.items[i]) < 0)
            {
                fprintf(stderr, "rm: failed to delete directory %s: %s\n",
                        dirs.items[i], strerror(errno));
            }
        }

        free_path_list(&files);
        free_path_list(&dirs);
        return 0;
    }
    else
    {
        return unlink(path_name);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: rm files...\n");
        return 1;
    }

    int flags = 0;
    size_t file_arg_begin = 1;
    if ((argc > 2) && (argv[1][0] == '-') && (argv[1][1] == 'r'))
    {
        file_arg_begin++;
        flags |= FLAG_RECURSIVE;
    }

    for (size_t i = file_arg_begin; i < argc; i++)
    {
        int ret = rm(argv[i], flags);
        if (ret != 0)
        {
            fprintf(stderr, "rm: %s failed to delete\n", argv[i]);
            return ret;
        }
    }

    return 0;
}
