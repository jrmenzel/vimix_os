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

struct sprintf_payload
{
    size_t n_left;
    char *dst_pos;
};

static void put_char_in_buffer(const int32_t c, size_t payload)
{
    struct sprintf_payload *my_payload = (struct sprintf_payload *)payload;

    // discarded chars are still counted (externally) for snprintf(), so no need
    // to return success
    if (my_payload->n_left == 0) return;

    *(my_payload->dst_pos) = (char)c;
    my_payload->dst_pos++;
    my_payload->n_left--;
}

int snprintf(char *dst, size_t n, const char *fmt, ...)
{
    struct sprintf_payload payload;
    payload.dst_pos = dst;
    payload.n_left = n;

    va_list ap;

    va_start(ap, fmt);
    int32_t ret = print_impl(put_char_in_buffer, (size_t)&payload, fmt, ap);
    va_end(ap);

    // 0-terminate, does not count as written char!
    // n == 0 means nothing can be written -> skip
    if (n > 0)
    {
        if (payload.n_left == 0)
        {
            *(payload.dst_pos - 1) = 0;
        }
        else
        {
            *(payload.dst_pos) = 0;
        }
    }

    return ret;
}
