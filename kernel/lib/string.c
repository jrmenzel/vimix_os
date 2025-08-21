/* SPDX-License-Identifier: MIT */

#include <kernel/kernel.h>
#include <kernel/string.h>

void *memset(void *dst, int c, size_t n)
{
    // copy unaligned bytes:
    char *pos = (char *)dst;
    while ((size_t)pos % sizeof(size_t) != 0 && n > 0)
    {
        *pos++ = (char)c;
        n--;
    }

    // copy as many as possible as aligned size_t memory accesses
    size_t *dst_s = (size_t *)pos;
    size_t n_word = n / sizeof(size_t);
    size_t remaining_bytes = n % sizeof(size_t);
    size_t value = 0;
    for (size_t i = 0; i < sizeof(size_t); ++i)
    {
        value = value << 8 | (char)c;
    }
    for (size_t i = 0; i < n_word; i++)
    {
        dst_s[i] = value;
    }
    pos += n_word * sizeof(size_t);

    // trailing unaligned bytes:
    for (size_t i = 0; i < remaining_bytes; i++)
    {
        *pos++ = (char)c;
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
    if (n > 0) dst[n - 1] = 0;
    return ret;
}

size_t strlen(const char *str)
{
    size_t n;

    for (n = 0; str[n]; ++n)
    {
    };

    return n;
}

size_t strnlen(const char *str, size_t maxlen)
{
    size_t n;

    for (n = 0; maxlen != 0 && str[n]; ++n, --maxlen)
    {
    };

    return n;
}

void *memchr(const void *s, int c, size_t n)
{
    // string and char are treated as unsigned char according to the specs
    unsigned char target = (unsigned char)c;
    unsigned char *string = (unsigned char *)s;

    for (size_t i = 0; i < n; ++i)
    {
        if (*string == target) return string;
        string++;
    }
    return NULL;
}

char *strrchr(const char *s, int c)
{
    size_t len = strlen(s);

    // start at len as finding the 0 terminator is valid if c == 0
    for (ssize_t i = len; i >= 0; --i)
    {
        if (s[i] == (char)c) return (char *)&(s[i]);
    }
    return NULL;
}

int _is_whitespace(char c)
{
    // \t = 9, \n = 10, \v = 11, \f = 12, \r = 13
    return (('\t' <= c && c <= '\r') || (c == ' '));
}

// from stdlib for libfdt:
unsigned long strtoul(const char *string, char **end, int base)
{
    if (base != 10) return 0;

    while (_is_whitespace(*string))
    {
        string++;
    }

    char sign = '+';
    if (*string == '-' || *string == '+')
    {
        sign = *string++;
    }

    unsigned long n = 0;
    while ('0' <= *string && *string <= '9')
    {
        n = n * 10 + *string++ - '0';
    }

    if (end)
    {
        *end = (char *)string;
    }

    if (sign == '-')
    {
        n = n * (-1);
    }
    return n;
}

char *strstr(const char *haystack, const char *needle)
{
    if ((haystack == NULL) || (needle == NULL)) return NULL;

    for (const char *start = haystack; *start != 0; start++)
    {
        const char *pin = needle;
        const char *hay = start;
        while ((*hay == *pin))
        {
            hay++;
            pin++;
        }

        if (*pin != 0)
        {
            // needle not fully found at this pos
            continue;
        }
        return (char *)start;
    }
    return NULL;
}
