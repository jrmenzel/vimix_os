/* SPDX-License-Identifier: MIT */
#include "print_impl.h"

#include <kernel/types.h>

/// single digit int32_t to ASCII char
#define INT_2_ASCII(x) (x + 0x30)

#if defined(_arch_is_32bit)
// 32 bit
#define _MAX_PRINT_INT_DEC_VALUE 1000000000
#define _MAX_PRINT_UINT_DEC_VALUE 1000000000
#define _MAX_PRINT_UINT_HEX_VALUE 0x10000000
#else
// 64 bit
#define _MAX_PRINT_INT_DEC_VALUE 1000000000000000000
#define _MAX_PRINT_UINT_DEC_VALUE 1000000000000000000
#define _MAX_PRINT_UINT_HEX_VALUE 0x1000000000000000
#endif

int32_t print_signed_long_long(_PUT_CHAR_FP func, size_t payload,
                               long long value)
{
    // 64bit:
    // -9,223,372,036,854,775,808..9,223,372,036,854,775,807
    // 32bit:
    // -2.147.483.648..2.147.483.647

    int32_t charsWritten = 0;

    if (value < 0)
    {
        func('-', payload);
        charsWritten++;
    }

    bool print = false;
    for (size_t i = _MAX_PRINT_INT_DEC_VALUE; i > 0; i /= 10)
    {
        if (i == 1)
        {
            print = true;
        }

        long long tmp = value / i;
        long long rem = tmp % 10;
        if (rem < 0)
        {
            rem *= -1;
        }
        if (rem > 0 || print)
        {
            print = true;
            func(INT_2_ASCII(rem), payload);
            charsWritten++;
        }
    }

    return charsWritten;
}

static const inline int32_t print_signed_int(_PUT_CHAR_FP func, size_t payload,
                                             int value)
{
    return print_signed_long_long(func, payload, (long long)value);
}

static const inline int32_t print_signed_long(_PUT_CHAR_FP func, size_t payload,
                                              long value)
{
    return print_signed_long_long(func, payload, (long long)value);
}

int32_t print_unsigned_long_long(_PUT_CHAR_FP func, size_t payload,
                                 unsigned long long value)
{
    int32_t charsWritten = 0;

    bool print = false;
    for (size_t i = _MAX_PRINT_UINT_DEC_VALUE; i > 0; i /= 10)
    {
        if (i == 1)
        {
            print = true;
        }

        unsigned long long tmp = value / i;
        unsigned long long rem = tmp % 10;
        if (rem > 0 || print)
        {
            print = true;
            func(INT_2_ASCII(rem), payload);
            charsWritten++;
        }
    }

    return charsWritten;
}

static const inline int32_t print_unsigned_int(_PUT_CHAR_FP func,
                                               size_t payload,
                                               unsigned int value)
{
    return print_unsigned_long_long(func, payload, (unsigned long long)value);
}

static const inline int32_t print_unsigned_long(_PUT_CHAR_FP func,
                                                size_t payload,
                                                unsigned long value)
{
    return print_unsigned_long_long(func, payload, (unsigned long long)value);
}

int32_t print_unsigned_hex(_PUT_CHAR_FP func, size_t payload, size_t value,
                           bool upperCase)
{
    int32_t charsWritten = 0;

    int32_t asciiOffset = upperCase ? 'A' : 'a';

    bool print = false;
    for (size_t i = _MAX_PRINT_UINT_HEX_VALUE; i > 0; i /= 16)
    {
        if (i == 1)
        {
            print = true;
        }

        size_t tmp = value / i;
        size_t rem = tmp % 16;
        if (rem > 0 || print)
        {
            print = true;
            if (rem <= 9)
            {
                func(INT_2_ASCII(rem), payload);
            }
            else
            {
                func(((rem - 10) + asciiOffset), payload);
            }
            charsWritten++;
        }
    }

    return charsWritten;
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
                            print_signed_int(func, payload, va_arg(vl, int));
                        break;
                    case 1:
                        charsWritten +=
                            print_signed_long(func, payload, va_arg(vl, long));
                        break;
                    case 2:
                    default:
                        charsWritten += print_signed_long_long(
                            func, payload, va_arg(vl, long long));
                }
            }
            else if (*format == 'u')
            {
                switch (length_mod)
                {
                    case 0:
                        charsWritten += print_unsigned_int(
                            func, payload, va_arg(vl, unsigned int));
                        break;
                    case 1:
                        charsWritten += print_unsigned_long(
                            func, payload, va_arg(vl, unsigned long));
                        break;
                    case 2:
                    default:
                        charsWritten += print_unsigned_long_long(
                            func, payload, va_arg(vl, unsigned long long));
                }
            }
            else if ((*format == 'x') || (*format == 'X'))
            {
                unsigned int value = va_arg(vl, unsigned int);
                charsWritten +=
                    print_unsigned_hex(func, payload, (size_t)value, false);
            }
            else if (*format == 'p')
            {
                void *value = va_arg(vl, void *);
                charsWritten +=
                    print_unsigned_hex(func, payload, (size_t)value, false);
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
