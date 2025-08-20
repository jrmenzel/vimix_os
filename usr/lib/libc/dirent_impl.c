/* SPDX-License-Identifier: MIT */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

DIR *opendir(const char *name)
{
    int fd = open(name, O_RDONLY);
    if (fd < 0) return NULL;

    return fdopendir(fd);
}

DIR *fdopendir(int fd)
{
    struct stat st;
    if (fstat(fd, &st) < 0) return NULL;

    if (!S_ISDIR(st.st_mode)) return NULL;

    DIR *dir = malloc(sizeof(DIR));
    if (dir == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    dir->next_entry = 0;
    dir->fd = fd;

    return dir;
}

struct dirent *readdir(DIR *dirp)
{
    if (dirp->next_entry < 0) return NULL;

    ssize_t res = get_dirent(dirp->fd, &(dirp->dir_entry), dirp->next_entry);
    if (res < 0)
    {
        // error
        return NULL;
    }
    if (res == 0)
    {
        dirp->next_entry = -1;  // invalid (until rewinddir())
        return NULL;
    }
    else
    {
        dirp->next_entry = res;
    }

    return &(dirp->dir_entry);
}

void rewinddir(DIR *dirp) { dirp->next_entry = 0; }

long telldir(DIR *dirp) { return dirp->next_entry; }

void seekdir(DIR *dirp, long loc) { dirp->next_entry = loc; }

int closedir(DIR *dirp)
{
    close(dirp->fd);
    free(dirp);

    return 0;
}
