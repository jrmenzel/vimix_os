/* SPDX-License-Identifier: MIT */

#include <stdarg.h>
#include <unistd.h>

// avoid dublicated code, re-use kernels libs.
#include "../../../kernel/lib/print_impl.c"

static void put_char_in_file(const int32_t c, size_t payload)
{
    char char_to_write = (char)c;
    write((FILE_DESCRIPTOR)payload, &char_to_write, 1);
}

int32_t fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int32_t ret = print_impl(put_char_in_file, (size_t)stream->fd, fmt, ap);
    va_end(ap);

    return ret;
}

int32_t printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int32_t ret = print_impl(put_char_in_file, (size_t)stdout->fd, fmt, ap);
    va_end(ap);

    return ret;
}
