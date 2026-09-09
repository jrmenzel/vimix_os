/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
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
        fprintf(stderr, "Usage: mv source destination\n");
        return 1;
    }

    char to_path[PATH_MAX];
    if (build_copy_target(argv[1], argv[2], to_path, PATH_MAX) != 0)
    {
        return 1;
    }

    errno = 0;
    if (rename(argv[1], to_path) < 0)
    {
        if (errno == EXDEV)
        {
            return move_file_across_filesystems(argv[1], to_path);
        }
        else
        {
            fprintf(stderr, "mv: cannot move %s to %s: %s\n", argv[1], to_path,
                    strerror(errno));
            return 1;
        }
    }
    return 0;
}
