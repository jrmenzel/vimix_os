/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(BUILD_ON_HOST)
#include <linux/limits.h>
#else
#include <kernel/limits.h>
#endif

#include "libvimixfs.h"

int main(int argc, char* argv[])
{
    const char* fs_filename = NULL;
    const char* dir_to_copy = NULL;
    size_t fs_size = 0;
    struct vimixfs_copy_params cp_in_params = {0};
    cp_in_params.fmode = 0644 | S_IFREG;
    cp_in_params.dmode = 0755 | S_IFDIR;

    enum operation_mode
    {
        MODE_CREATE,
        MODE_COPY_IN,
        MODE_COPY_OUT,
        MODE_CHANGE_METADATA
    } mode = MODE_CREATE;

    for (size_t i = 1; i < argc;)
    {
        if ((strcmp(argv[i], "--fs") == 0) && (i + 1 < argc))
        {
            fs_filename = argv[i + 1];
            i += 2;
        }
        else if ((strcmp(argv[i], "--create") == 0) && (i + 1 < argc))
        {
            fs_size = atoi(argv[i + 1]);
            mode = MODE_CREATE;
            i += 2;
        }
        else if ((strcmp(argv[i], "--in") == 0) && (i + 2 < argc))
        {
            // --in: copy a directory from host into the fs
            cp_in_params.src_path_on_host = argv[i + 1];
            cp_in_params.dst_dir_on_fs = argv[i + 2];
            mode = MODE_COPY_IN;
            i += 3;
        }
        else if ((strcmp(argv[i], "--out") == 0) && (i + 1 < argc))
        {
            // --out: copy everything from the fs into a directory on host
            dir_to_copy = argv[i + 1];
            mode = MODE_COPY_OUT;
            i += 2;
        }
        else if ((strcmp(argv[i], "--meta") == 0) && (i + 1 < argc))
        {
            // change metadata
            cp_in_params.dst_dir_on_fs = argv[i + 1];
            mode = MODE_CHANGE_METADATA;
            i += 2;
        }
        else if ((strcmp(argv[i], "--uid") == 0) && (i + 1 < argc))
        {
            // user id for copied files
            cp_in_params.uid = atoi(argv[i + 1]);
            i += 2;
        }
        else if ((strcmp(argv[i], "--gid") == 0) && (i + 1 < argc))
        {
            // group id for copied files
            cp_in_params.gid = atoi(argv[i + 1]);
            i += 2;
        }
        else if ((strcmp(argv[i], "--fmode") == 0) && (i + 1 < argc))
        {
            // file mode for copied files
            cp_in_params.fmode = (mode_t)strtoul(argv[i + 1], NULL, 8);
            cp_in_params.fmode |= S_IFREG;
            i += 2;
        }
        else if ((strcmp(argv[i], "--dmode") == 0) && (i + 1 < argc))
        {
            // dir mode for copied dirs
            cp_in_params.dmode = (mode_t)strtoul(argv[i + 1], NULL, 8);
            cp_in_params.dmode |= S_IFDIR;
            i += 2;
        }
        else
        {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            exit(1);
        }
    }

    struct vimixfs fs_file;
    bool ok = false;
    switch (mode)
    {
        case MODE_CREATE:
        {
            ok = vimixfs_create(&fs_file, fs_filename, fs_size);
            if (!ok)
            {
                fprintf(stderr, "ERROR creating %s\n", fs_filename);
                exit(EXIT_FAILURE);
            }

            vimixfs_close(&fs_file);
            break;
        }
        case MODE_COPY_IN:
        {
            if (!vimixfs_open(&fs_file, fs_filename))
            {
                fprintf(stderr, "ERROR opening %s\n", fs_filename);
                exit(EXIT_FAILURE);
            }

            ok =
                vimixfs_cp_from_host(&fs_file, cp_in_params.src_path_on_host,
                                     cp_in_params.dst_dir_on_fs, &cp_in_params);

            vimixfs_close(&fs_file);
            break;
        }
        case MODE_COPY_OUT:
        {
            if (!vimixfs_open(&fs_file, fs_filename))
            {
                fprintf(stderr, "ERROR opening %s\n", fs_filename);
                exit(EXIT_FAILURE);
            }
            ok = vimixfs_cp_dir_to_host(&fs_file, VIMIXFS_ROOT_INODE, "/",
                                        dir_to_copy);
            vimixfs_close(&fs_file);
            break;
        }
        case MODE_CHANGE_METADATA:
        {
            if (!vimixfs_open(&fs_file, fs_filename))
            {
                fprintf(stderr, "ERROR opening %s\n", fs_filename);
                exit(EXIT_FAILURE);
            }
            ok = vimixfs_change_metadata(&fs_file, cp_in_params.dst_dir_on_fs,
                                         &cp_in_params);
            vimixfs_close(&fs_file);
            break;
        }
    }

    if (!ok)
    {
        fprintf(stderr, "Usage: mkfs --fs fs.img [--in|--out] <dir>\n");
        fprintf(stderr,
                "       mkfs --fs fs.img --create <size in blocks/kb>\n");
    }

    return (ok ? 0 : 1);
}
