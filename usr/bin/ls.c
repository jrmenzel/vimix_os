/* SPDX-License-Identifier: MIT */

#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/stat.h>
#include <user.h>

char *fmtname(char *path)
{
    static char buf[XV6_NAME_MAX + 1];
    char *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // Return blank-padded name.
    if (strlen(p) >= XV6_NAME_MAX) return p;
    memmove(buf, p, strlen(p));
    memset(buf + strlen(p), ' ', XV6_NAME_MAX - strlen(p));
    return buf;
}

void ls(char *path)
{
    char buf[512], *p;
    int fd;
    struct xv6fs_dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "ls: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
        case XV6_FT_DEVICE:
        case XV6_FT_FILE:
            printf("%s %d %d %l\n", fmtname(path), st.type, st.st_ino,
                   st.st_size);
            break;

        case XV6_FT_DIR:
            if (strlen(path) + 1 + XV6_NAME_MAX + 1 > sizeof buf)
            {
                printf("ls: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            while (read(fd, &de, sizeof(de)) == sizeof(de))
            {
                if (de.inum == 0) continue;
                memmove(p, de.name, XV6_NAME_MAX);
                p[XV6_NAME_MAX] = 0;
                if (stat(buf, &st) < 0)
                {
                    printf("ls: cannot stat %s\n", buf);
                    continue;
                }
                printf("%s %d %d %d\n", fmtname(buf), st.type, st.st_ino,
                       st.st_size);
            }
            break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        ls(".");
        exit(0);
    }
    for (size_t i = 1; i < argc; i++)
    {
        ls(argv[i]);
    }
    exit(0);
}
