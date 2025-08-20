/* SPDX-License-Identifier: MIT */

// Simple grep.  Only supports ^ . * $ operators.

#pragma GCC diagnostic ignored "-Wanalyzer-infinite-recursion"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char buf[1024];
int match(char *, char *);

void grep(char *pattern, int fd)
{
    int n, m;
    char *p, *q;

    m = 0;
    while ((n = read(fd, buf + m, sizeof(buf) - m - 1)) > 0)
    {
        m += n;
        buf[m] = '\0';
        p = buf;
        while ((q = strchr(p, '\n')) != 0)
        {
            *q = 0;
            if (match(pattern, p))
            {
                *q = '\n';
                ssize_t written = write(1, p, q + 1 - p);
                if (written != q + 1 - p) exit(1);
            }
            p = q + 1;
        }
        if (m > 0)
        {
            m -= p - buf;
            memmove(buf, p, m);
        }
    }
}

int main(int argc, char *argv[])
{
    char *pattern;

    if (argc <= 1)
    {
        fprintf(stderr, "usage: grep pattern [file ...]\n");
        return 1;
    }
    pattern = argv[1];

    if (argc <= 2)
    {
        grep(pattern, 0);
        return 0;
    }

    for (size_t i = 2; i < argc; i++)
    {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0)
        {
            printf("grep: cannot open %s\n", argv[i]);
            return 1;
        }
        grep(pattern, fd);
        close(fd);
    }

    return 0;
}

// Regexp matcher from Kernighan & Pike,
// The Practice of Programming, Chapter 9, or
// https://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html

int matchhere(char *, char *);
int matchstar(int, char *, char *);

int match(char *re, char *text)
{
    if (re[0] == '^') return matchhere(re + 1, text);
    do {  // must look at empty string
        if (matchhere(re, text)) return 1;
    } while (*text++ != '\0');
    return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
    if (re[0] == '\0') return 1;
    if (re[1] == '*') return matchstar(re[0], re + 2, text);
    if (re[0] == '$' && re[1] == '\0') return *text == '\0';
    if (*text != '\0' && (re[0] == '.' || re[0] == *text))
        return matchhere(re + 1, text + 1);
    return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
    do {  // a * matches zero or more instances
        if (matchhere(re, text)) return 1;
    } while (*text != '\0' && (*text++ == c || c == '.'));
    return 0;
}
