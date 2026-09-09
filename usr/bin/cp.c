/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <vimixutils/file.h>
#include <vimixutils/path.h>

#if defined(BUILD_ON_HOST)
#include <linux/limits.h>
#else
#include <kernel/limits.h>
#endif

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: cp from to\n");
        return 1;
    }

    char to_path[PATH_MAX];
    if (build_copy_target(argv[1], argv[2], to_path, PATH_MAX) != 0)
    {
        return 1;
    }

    return copy_file(argv[1], to_path);
}
