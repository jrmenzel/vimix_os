/* SPDX-License-Identifier: MIT */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vimixutils/minmax.h>
#include <vimixutils/sysfs.h>

void print_trailing_spaces(size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        printf(" ");
    }
}

void print_line(const char *line_prefix, size_t size)
{
    ssize_t printed = printf("%s:", line_prefix);
    print_trailing_spaces(max(14 - printed, 0));

    printf("%10zu bytes (%8zu kb)\n", size, size / 1024);
}

void print_cache(const char *cache_name)
{
    char path[PATH_MAX];

    snprintf(path, PATH_MAX, "/sys/kmem/%s/slab_count", cache_name);
    size_t slab_count = get_from_sysfs(path);

    snprintf(path, PATH_MAX, "/sys/kmem/%s/obj_size", cache_name);
    size_t obj_size = get_from_sysfs(path);

    snprintf(path, PATH_MAX, "/sys/kmem/%s/obj_count", cache_name);
    size_t obj_count = get_from_sysfs(path);

    snprintf(path, PATH_MAX, "/sys/kmem/%s/obj_max", cache_name);
    size_t obj_max = get_from_sysfs(path);

    size_t printed = printf("Cache %s: ", cache_name);
    print_trailing_spaces(max(20 - printed, 0));
    printf("%4zu /%4zu objects of size %4zu bytes (%3zu kb used), %2zu slabs\n",
           obj_count, obj_max, obj_size, (obj_size * obj_count) / 1024,
           slab_count);
}

void print_caches()
{
    DIR *dir = opendir("/sys/kmem");
    if (dir == NULL)
    {
        return;
    }

    struct dirent *dir_entry = NULL;
    while ((dir_entry = readdir(dir)))
    {
        // for all entries:
        if (strncmp(dir_entry->d_name, "kmalloc_", 8) != 0)
        {
            // file name does not begin with kmalloc_
            continue;
        }

        print_cache(dir_entry->d_name);
    }

    closedir(dir);
}

void print_bio_cache()
{
    size_t num = get_from_sysfs("/sys/kmem/bio/num");
    size_t free = get_from_sysfs("/sys/kmem/bio/free");
    size_t min = get_from_sysfs("/sys/kmem/bio/min");
    size_t max_free = get_from_sysfs("/sys/kmem/bio/max_free");

    size_t printed = printf("Block IO cache: ");
    print_trailing_spaces(max(20 - printed, 0));
    printf("%zu buffers, %zu free; min: %zu; max free: %zu\n", num, free, min,
           max_free);
}

int main(int argc, char *argv[])
{
    size_t mem_total = get_from_sysfs("/sys/kmem/mem_total");
    size_t mem_free = get_from_sysfs("/sys/kmem/mem_free");
    size_t pages_alloc = get_from_sysfs("/sys/kmem/pages_alloc");

    long page_size = sysconf(_SC_PAGE_SIZE);

    print_line("Total memory", mem_total);
    print_line("Free memory", mem_free);
    print_line("Used memory", mem_total - mem_free);
    print_line("Allocated", pages_alloc * page_size);

    print_bio_cache();
    print_caches();

    return 0;
}
