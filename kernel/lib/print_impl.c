/* SPDX-License-Identifier: MIT */
#include "print_impl.h"

#include <kernel/types.h>

/// single digit int32_t to ASCII char
#define INT_2_ASCII(x) (x + 0x30)

#define ASCII_2_INT(x) (x - 0x30)

int32_t print_number_buffer(_PUT_CHAR_FP func, size_t payload, char *buffer,
                            size_t max_len, ssize_t padding)
{
    int32_t charsWritten = 0;

    padding = (padding <= 0) ? 1 : padding;
    bool print = false;
    for (size_t i = 0; i < max_len; ++i)
    {
        if (buffer[i] != '0' || i == max_len - padding)
        {
            // start printing at the first non zero char or the last one
            print = true;
        }

        if (print)
        {
            func(buffer[i], payload);
            charsWritten++;
        }
    }
    return charsWritten;
}

int32_t print_signed_long_long(_PUT_CHAR_FP func, size_t payload,
                               ssize_t padding, long long value)
{
    int32_t charsWritten = 0;

    if (value < 0)
    {
        func('-', payload);
        charsWritten++;
        padding--;
    }

#if defined(_arch_is_32bit)
    const size_t MAX_LEN = 10;  // -2,147,483,648..2,147,483,647
#else
    const size_t MAX_LEN =
        19;  // -9,223,372,036,854,775,808..9,223,372,036,854,775,807
#endif
    char buffer[MAX_LEN];

    for (size_t i = 0; i < MAX_LEN; ++i)
    {
        long long rem = value % 10;
        value = value / 10;
        if (rem < 0) rem *= -1;
        buffer[MAX_LEN - i - 1] = INT_2_ASCII(rem);
    }

    return charsWritten +
           print_number_buffer(func, payload, buffer, MAX_LEN, padding);
    /*
        padding = (padding <= 0) ? 1 : padding;
        bool print = false;
        for (size_t i = 0; i < MAX_LEN; ++i)
        {
            if (buffer[i] != '0' || i == MAX_LEN - padding)
            {
                // start printing at the first non zero char or the last one
                print = true;
            }

            if (print)
            {
                func(buffer[i], payload);
                charsWritten++;
            }
        }
        return charsWritten;*/
}

static const inline int32_t print_signed_int(_PUT_CHAR_FP func, size_t payload,
                                             ssize_t padding, int value)
{
    return print_signed_long_long(func, payload, padding, (long long)value);
}

static const inline int32_t print_signed_long(_PUT_CHAR_FP func, size_t payload,
                                              ssize_t padding, long value)
{
    return print_signed_long_long(func, payload, padding, (long long)value);
}

int32_t print_unsigned_long_long(_PUT_CHAR_FP func, size_t payload,
                                 ssize_t padding, unsigned long long value)
{
    int32_t charsWritten = 0;

#if defined(_arch_is_32bit)
    const size_t MAX_LEN = 10;  // 4,294,967,295
#else
    const size_t MAX_LEN = 20;  // 18,446,744,073,709,551,615
#endif
    char buffer[MAX_LEN];

    for (size_t i = 0; i < MAX_LEN; ++i)
    {
        long long rem = value % 10;
        value = value / 10;
        if (rem < 0) rem *= -1;
        buffer[MAX_LEN - i - 1] = INT_2_ASCII(rem);
    }

    return charsWritten +
           print_number_buffer(func, payload, buffer, MAX_LEN, padding);
    /*

        padding = (padding <= 0) ? 1 : padding;
        bool print = false;
        for (size_t i = 0; i < MAX_LEN; ++i)
        {
            if (buffer[i] != '0' || i == MAX_LEN - padding)
            {
                // start printing at the first non zero char or the last one
                print = true;
            }

            if (print)
            {
                func(buffer[i], payload);
                charsWritten++;
            }
        }
        return charsWritten;*/
}

static const inline int32_t print_unsigned_int(_PUT_CHAR_FP func,
                                               size_t payload, ssize_t padding,
                                               unsigned int value)
{
    return print_unsigned_long_long(func, payload, padding,
                                    (unsigned long long)value);
}

static const inline int32_t print_unsigned_long(_PUT_CHAR_FP func,
                                                size_t payload, ssize_t padding,
                                                unsigned long value)
{
    return print_unsigned_long_long(func, payload, padding,
                                    (unsigned long long)value);
}

int32_t print_unsigned_hex(_PUT_CHAR_FP func, size_t payload, ssize_t padding,
                           size_t value, bool upperCase)
{
    int32_t asciiOffset = upperCase ? 'A' : 'a';
    int32_t charsWritten = 0;

#if defined(_arch_is_32bit)
    const size_t MAX_LEN = 8;  // FFFF FFFF
#else
    const size_t MAX_LEN = 16;  // FFFF FFFF FFFF FFFF
#endif
    char buffer[MAX_LEN];

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
    }

    return charsWritten +
           print_number_buffer(func, payload, buffer, MAX_LEN, padding);
    /*

    padding = (padding <= 0) ? 1 : padding;
    bool print = false;
    for (size_t i = 0; i < MAX_LEN; ++i)
    {
        if (buffer[i] != '0' || i == MAX_LEN - padding)
        {
            // start printing at the first non zero char or the last one
            print = true;
        }

        if (print)
        {
            func(buffer[i], payload);
            charsWritten++;
        }
    }
    return charsWritten;*/
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
            func('\r', payload);
            format++;
        }
        else if (*format == '%')
        {
            format++;
            if (*format == 0)
            {
                break;
            }

            ssize_t padding = 0;
            if (*format == '0')
            {
                // one or two chars defining the padding amount
                format++;
                if (*format == 0) break;
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
                        charsWritten += print_signed_int(func, payload, padding,
                                                         va_arg(vl, int));
                        break;
                    case 1:
                        charsWritten += print_signed_long(
                            func, payload, padding, va_arg(vl, long));
                        break;
                    case 2:
                    default:
                        charsWritten += print_signed_long_long(
                            func, payload, padding, va_arg(vl, long long));
                }
            }
            else if (*format == 'u')
            {
                switch (length_mod)
                {
                    case 0:
                        charsWritten += print_unsigned_int(
                            func, payload, padding, va_arg(vl, unsigned int));
                        break;
                    case 1:
                        charsWritten += print_unsigned_long(
                            func, payload, padding, va_arg(vl, unsigned long));
                        break;
                    case 2:
                    default:
                        charsWritten += print_unsigned_long_long(
                            func, payload, padding,
                            va_arg(vl, unsigned long long));
                }
            }
            else if ((*format == 'x') || (*format == 'X'))
            {
                size_t value = va_arg(vl, size_t);
                charsWritten += print_unsigned_hex(
                    func, payload, padding, (size_t)value, (*format == 'X'));
            }
            else if (*format == 'p')
            {
                void *value = va_arg(vl, void *);
                charsWritten += print_unsigned_hex(func, payload, padding,
                                                   (size_t)value, false);
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
