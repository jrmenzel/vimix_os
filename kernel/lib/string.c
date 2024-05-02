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
    const uint8_t *s1 = v1;
    const uint8_t *s2 = v2;
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
    char *_dst;
    const char *_src;

    _dst = dst;
    _src = src;
    if (_src > _dst)
    {
        while (n-- > 0) *_dst++ = *_src++;
    }
    else
    {
        _dst += n;
        _src += n;
        while (n-- > 0) *--_dst = *--_src;
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
    return (uint8_t)*s1 - (uint8_t)*s2;
}

char *strcpy(char *dst, const char *src)
{
    char *input_dst = dst;
    while ((*dst++ = *src++) != 0)
    {
    }
    return input_dst;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n > 0 && *s1 && *s1 == *s2)
    {
        n--;
        s1++;
        s2++;
    }
    if (n == 0)
    {
        return 0;
    }
    return (uint8_t)*s1 - (uint8_t)*s2;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    char *input_dst = dst;

    while (n)
    {
        n--;
        // copy char and test for null terminator
        const char current_char = (*dst++ = *src++);
        if (current_char == '\0')
        {
            break;
        }
    }

    // fill string with 0
    while (n)
    {
        n--;
        *dst++ = 0;
    }

    return input_dst;
}

char *safestrcpy(char *dst, const char *src, size_t n)
{
    char *ret = strncpy(dst, src, n);
    dst[n - 1] = 0;
    return ret;
}

size_t strlen(const char *str)
{
    size_t n;

    for (n = 0; str[n]; n++)
    {
    };

    return n;
}
