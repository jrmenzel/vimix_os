/* SPDX-License-Identifier: MIT */
#include "print_impl.h"

#include <kernel/types.h>

/// single digit int32_t to ASCII char
#define INT_2_ASCII(x) (x + 0x30)

#define ASCII_2_INT(x) (x - 0x30)

int32_t print_signed_long_long(_PUT_CHAR_FP func, size_t payload,
                               ssize_t padding, char padding_char,
                               long long value)
{
    bool print_minus = false;
    if (value < 0)
    {
        print_minus = true;
        value *= -1;
    }

#if defined(__ARCH_32BIT)
    const size_t MAX_LEN = 10;  // -2,147,483,648..2,147,483,647
#else
    const size_t MAX_LEN =
        19;  // -9,223,372,036,854,775,808..9,223,372,036,854,775,807
#endif
    char buffer[MAX_LEN];

    size_t num_len = 0;
    for (size_t i = 0; i < MAX_LEN; ++i)
    {
        long long rem = value % 10;
        value = value / 10;
        if (rem < 0) rem *= -1;
        buffer[MAX_LEN - i - 1] = INT_2_ASCII(rem);

        if (value == 0)
        {
            num_len = i + 1;
            break;
        }
    }
    if (print_minus && padding_char == ' ')
    {
        // print after the number, before the padding whitespace
        buffer[MAX_LEN - num_len - 1] = '-';
        num_len++;
    }
    if (padding > num_len)
    {
        padding -= num_len;
        if (print_minus && padding_char == '0')
        {
            padding--;
        }
        while (padding > 0)
        {
            buffer[MAX_LEN - num_len - 1] = padding_char;
            num_len++;
            padding--;
        }
    }
    if (print_minus && padding_char == '0')
    {
        // print after the 0-padde number
        buffer[MAX_LEN - num_len - 1] = '-';
        num_len++;
    }

    for (size_t i = 0; i < num_len; ++i)
    {
        func(buffer[MAX_LEN - num_len + i], payload);
    }
    return num_len;
}

static const inline int32_t print_signed_int(_PUT_CHAR_FP func, size_t payload,
                                             ssize_t padding, char padding_char,
                                             int value)
{
    return print_signed_long_long(func, payload, padding, padding_char,
                                  (long long)value);
}

static const inline int32_t print_signed_long(_PUT_CHAR_FP func, size_t payload,
                                              ssize_t padding,
                                              char padding_char, long value)
{
    return print_signed_long_long(func, payload, padding, padding_char,
                                  (long long)value);
}

int32_t print_unsigned_long_long(_PUT_CHAR_FP func, size_t payload,
                                 ssize_t padding, char padding_char,
                                 unsigned long long value)
{
#if defined(__ARCH_32BIT)
    const size_t MAX_LEN = 10;  // 4,294,967,295
#else
    const size_t MAX_LEN = 20;  // 18,446,744,073,709,551,615
#endif
    char buffer[MAX_LEN];

    size_t num_len = 0;
    for (size_t i = 0; i < MAX_LEN; ++i)
    {
        long long rem = value % 10;
        value = value / 10;
        if (rem < 0) rem *= -1;
        buffer[MAX_LEN - i - 1] = INT_2_ASCII(rem);

        if (value == 0)
        {
            num_len = i + 1;
            break;
        }
    }
    if (padding > num_len)
    {
        padding -= num_len;
        while (padding > 0)
        {
            buffer[MAX_LEN - num_len - 1] = padding_char;
            num_len++;
            padding--;
        }
    }

    for (size_t i = 0; i < num_len; ++i)
    {
        func(buffer[MAX_LEN - num_len + i], payload);
    }
    return num_len;
}

static const inline int32_t print_unsigned_int(_PUT_CHAR_FP func,
                                               size_t payload, ssize_t padding,
                                               char padding_char,
                                               unsigned int value)
{
    return print_unsigned_long_long(func, payload, padding, padding_char,
                                    (unsigned long long)value);
}

static const inline int32_t print_unsigned_long(_PUT_CHAR_FP func,
                                                size_t payload, ssize_t padding,
                                                char padding_char,
                                                unsigned long value)
{
    return print_unsigned_long_long(func, payload, padding, padding_char,
                                    (unsigned long long)value);
}

int32_t print_unsigned_hex(_PUT_CHAR_FP func, size_t payload, ssize_t padding,
                           char padding_char, size_t value, bool upperCase)
{
    int32_t asciiOffset = upperCase ? 'A' : 'a';

#if defined(__ARCH_32BIT)
    const size_t MAX_LEN = 8;  // FFFF FFFF
#else
    const size_t MAX_LEN = 16;  // FFFF FFFF FFFF FFFF
#endif
    char buffer[MAX_LEN];

    size_t num_len = 0;
    for (size_t i = 0; i < MAX_LEN; ++i)
    {
        size_t rem = value % 16;
        value = value / 16;
        if (rem <= 9)
        {
            buffer[MAX_LEN - i - 1] = INT_2_ASCII(rem);
        }
        else
        {
            buffer[MAX_LEN - i - 1] = (rem - 10) + asciiOffset;
        }

        if (value == 0)
        {
            num_len = i + 1;
            break;
        }
    }

    if (padding > num_len)
    {
        padding -= num_len;
        while (padding > 0)
        {
            buffer[MAX_LEN - num_len - 1] = padding_char;
            num_len++;
            padding--;
        }
    }

    for (size_t i = 0; i < num_len; ++i)
    {
        func(buffer[MAX_LEN - num_len + i], payload);
    }
    return num_len;
}

int32_t print_string(_PUT_CHAR_FP func, size_t payload, const char *value)
{
    int32_t charsWritten = 0;
    int32_t i = 0;
    while (value[i] != 0)
    {
        func(value[i], payload);
        i++;
        charsWritten++;
    }
    return charsWritten;
}

int32_t print_impl(_PUT_CHAR_FP func, size_t payload, const char *format,
                   va_list vl)
{
    int32_t charsWritten = 0;

    while (*format)
    {
        // \n handling
        if (*format == '\n')
        {
            func('\n', payload);
            charsWritten++;
            format++;
        }
        else if (*format == '%')
        {
            format++;
            if (*format == 0)
            {
                break;
            }

            // optional padding:
            // %[0][1-9][0-9]d -> start with 0 for 0 padding
            // then 1..99 for padding amount
            ssize_t padding = 0;
            char padding_char = ' ';
            if (*format >= '0' && *format < '9')
            {
                if (*format == '0')
                {
                    // 0 padding
                    format++;
                    if (*format == 0) break;
                    padding_char = '0';
                }
                // one or two chars define the padding amount
                padding = ASCII_2_INT(*format);
                if (padding > 9 || padding < 0) padding = 0;
                format++;
                if (*format == 0) break;
                if (*format >= '0' && *format < '9')
                {
                    padding = padding * 10 + ASCII_2_INT(*format);
                    format++;
                    if (*format == 0) break;
                }
            }

            // can be l or ll
            int32_t length_mod = 0;
            for (size_t i = 0; i < 2; ++i)
            {
                if (*format == 'l')
                {
                    format++;
                    length_mod++;
                }
            }
            if (*format == 'L')
            {
                format++;
                length_mod = 2;  // L = ll
            }
            if (*format == 'z')
            {
                format++;
                length_mod = 1;  // size_t = unsigned long int, 32 or 64 bit
            }

            if ((*format == 'd') || (*format == 'i'))
            {
                switch (length_mod)
                {
                    case 0:
                        charsWritten +=
                            print_signed_int(func, payload, padding,
                                             padding_char, va_arg(vl, int));
                        break;
                    case 1:
                        charsWritten +=
                            print_signed_long(func, payload, padding,
                                              padding_char, va_arg(vl, long));
                        break;
                    case 2:
                    default:
                        charsWritten += print_signed_long_long(
                            func, payload, padding, padding_char,
                            va_arg(vl, long long));
                }
            }
            else if (*format == 'u')
            {
                switch (length_mod)
                {
                    case 0:
                        charsWritten += print_unsigned_int(
                            func, payload, padding, padding_char,
                            va_arg(vl, unsigned int));
                        break;
                    case 1:
                        charsWritten += print_unsigned_long(
                            func, payload, padding, padding_char,
                            va_arg(vl, unsigned long));
                        break;
                    case 2:
                    default:
                        charsWritten += print_unsigned_long_long(
                            func, payload, padding, padding_char,
                            va_arg(vl, unsigned long long));
                }
            }
            else if ((*format == 'x') || (*format == 'X'))
            {
                size_t value = va_arg(vl, size_t);
                charsWritten +=
                    print_unsigned_hex(func, payload, padding, padding_char,
                                       (size_t)value, (*format == 'X'));
            }
            else if (*format == 'p')
            {
                void *value = va_arg(vl, void *);
                charsWritten += print_unsigned_hex(
                    func, payload, padding, padding_char, (size_t)value, false);
            }
            else if (*format == 's')
            {
                const char *value = va_arg(vl, const char *);
                charsWritten += print_string(func, payload, value);
            }
            else if (*format == 'c')
            {
                int32_t value = va_arg(vl, int32_t);
                charsWritten++;
                func(value, payload);
            }
            format++;
        }
        else
        {
            // printable char:
            func(*format, payload);
            format++;
            charsWritten++;
        }
    }

    return charsWritten;
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

int vsnprintf(char *dst, size_t n, const char *format, va_list arg)
{
    struct sprintf_payload payload;
    payload.dst_pos = dst;
    payload.n_left = n;

    int32_t ret = print_impl(put_char_in_buffer, (size_t)&payload, format, arg);

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

int snprintf(char *dst, size_t n, const char *format, ...)
{
    va_list arg;

    va_start(arg, format);
    int32_t ret = vsnprintf(dst, n, format, arg);
    va_end(arg);
    return ret;
}
