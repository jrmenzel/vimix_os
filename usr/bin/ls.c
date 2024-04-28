/* SPDX-License-Identifier: MIT */

#include <fcntl.h>
#include <kernel/fs.h>
#include <kernel/xv6fs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

char type_to_char(mode_t mode)
{
    if (S_ISBLK(mode)) return 'b';
    if (S_ISCHR(mode)) return 'c';
    if (S_ISDIR(mode)) return 'd';
    if (S_ISREG(mode)) return '.';
    if (S_ISFIFO(mode)) return 'p';
    return ' ';
}

void print_access(mode_t mode)
{
    char str[11] = ".rwxrwxrwx";
    str[0] = type_to_char(mode);

    size_t pos = 9;
    for (size_t i = 0; i < 9; ++i)
    {
        mode_t bit = mode & 0x01;
        mode = mode >> 1;

        if (!bit) str[pos] = '-';
        pos--;
    }

    printf(str);
}

void print_padded(size_t value, size_t min_chars_wide, bool min_one_space)
{
    size_t value_width = 1;
    size_t tmp = value;
    while ((tmp /= 10) > 0)
    {
        value_width++;
    }

    ssize_t missing_chars = min_chars_wide - value_width;
    if (min_one_space && missing_chars < 1)
    {
        missing_chars = 1;
    }
    if (missing_chars > 0)
    {
        for (size_t i = 0; i < missing_chars; ++i)
        {
            printf(" ");
        }
    }

    printf("%d", value);
}

void print_file(char *path, struct stat *st)
{
    print_padded(st->st_ino, 4, false);
    printf(" ");
    print_access(st->st_mode);

    print_padded(st->st_size, 8, true);
    printf(" B %s\n", fmtname(path));
}

void ls(char *path)
{
    int fd = open(path, 0);
    if (fd < 0)
    {
        fprintf(stderr, "ls: cannot open %s\n", path);
        return;
    }

    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        fprintf(stderr, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    if (S_ISREG(st.st_mode) || S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
    {
        print_file(path, &st);
    }
    else if (S_ISDIR(st.st_mode))
    {
        char buf[512], *p;
        if (strlen(path) + 1 + XV6_NAME_MAX + 1 > sizeof buf)
        {
            printf("ls: path too long\n");
            close(fd);
            return;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';

        struct xv6fs_dirent de;
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
            print_file(buf, &st);
        }
    }

    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        ls(".");
        return 0;
    }
    for (size_t i = 1; i < argc; i++)
    {
        ls(argv[i]);
    }

    return 0;
}
