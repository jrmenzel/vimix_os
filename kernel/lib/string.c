/* SPDX-License-Identifier: MIT */

#include <kernel/kernel.h>
#include <kernel/string.h>

void *memset(void *dst, int c, size_t n)
{
    char *cdst = (char *)dst;
    for (size_t i = 0; i < n; i++)
    {
        cdst[i] = (char)c;
    }
    return dst;
}

int memcmp(const void *v1, const void *v2, size_t n)
{
    const uchar *s1 = v1;
    const uchar *s2 = v2;
    while (n-- > 0)
    {
        if (*s1 != *s2)
        {
            return (*s1 - *s2);
        }
        s1++;
        s2++;
    }

    // memory was equal
    return 0;
}

void *memmove(void *dst, const void *src, size_t n)
{
    ssize_t signed_n = n;
    const char *s;
    char *d;

    if (signed_n == 0) return dst;

    s = src;
    d = dst;
    if (s < d && s + signed_n > d)
    {
        s += signed_n;
        d += signed_n;
        while (signed_n-- > 0) *--d = *--s;
    }
    else
    {
        while (signed_n-- > 0) *d++ = *s++;
    }

    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    return memmove(dst, src, n);
}

char *strchr(const char *str, char c)
{
    for (; *str; str++)
    {
        if (*str == c) return (char *)str;
    }
    return NULL;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2)
    {
        s1++;
        s2++;
    }
    return (uchar)*s1 - (uchar)*s2;
}

char *strcpy(char *dst, const char *src)
{
    char *input_dst = dst;
    while ((*dst++ = *src++) != 0)
    {
    }
    return input_dst;
}

int strncmp(const char *p, const char *q, size_t n)
{
    while (n > 0 && *p && *p == *q) n--, p++, q++;
    if (n == 0) return 0;
    return (uchar)*p - (uchar)*q;
}

char *strncpy(char *s, const char *t, size_t n)
{
    char *input_dst = s;

    while (n)
    {
        n--;
        // copy char and test for null terminator
        const char current_char = (*s++ = *t++);
        if (current_char == '\0')
        {
            break;
        }
    }

    // fill string with 0
    while (n)
    {
        n--;
        *s++ = 0;
    }

    return input_dst;
}

/// Like strncpy but guaranteed to NUL-terminate.
char *safestrcpy(char *s, const char *t, size_t n)
{
    char *os;

    os = s;
    if (n <= 0) return os;
    while (--n > 0 && (*s++ = *t++) != 0)
        ;
    *s = 0;
    return os;
}

size_t strlen(const char *s)
{
    size_t n;

    for (n = 0; s[n]; n++)
        ;
    return n;
}
