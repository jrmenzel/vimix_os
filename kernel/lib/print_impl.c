/* SPDX-License-Identifier: MIT */
#include "print_impl.h"

#include <kernel/types.h>

/// single digit int32_t to ASCII char
#define INT_2_ASCII(x) (x + 0x30)

#if defined(_arch_is_32bit)
// 32 bit
typedef int32_t _printIntType;
typedef uint32_t _printUIntType;
#define _MAX_PRINT_INT_DEC_VALUE 1000000000
#define _MAX_PRINT_UINT_DEC_VALUE 1000000000
#define _MAX_PRINT_UINT_HEX_VALUE 0x10000000
#else
// 64 bit
typedef int64_t _printIntType;
typedef uint64_t _printUIntType;
#define _MAX_PRINT_INT_DEC_VALUE 1000000000000000000
#define _MAX_PRINT_UINT_DEC_VALUE 1000000000000000000
#define _MAX_PRINT_UINT_HEX_VALUE 0x1000000000000000
#endif

int32_t print_signed_int(_PUT_CHAR_FP func, size_t payload, _printIntType value)
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
    for (_printIntType i = _MAX_PRINT_INT_DEC_VALUE; i > 0; i /= 10)
    {
        if (i == 1)
        {
            print = true;
        }

        _printIntType tmp = value / i;
        _printIntType rem = tmp % 10;
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

int32_t print_unsigned_int(_PUT_CHAR_FP func, size_t payload,
                           _printUIntType value)
{
    int32_t charsWritten = 0;

    bool print = false;
    for (_printUIntType i = _MAX_PRINT_UINT_DEC_VALUE; i > 0; i /= 10)
    {
        if (i == 1)
        {
            print = true;
        }

        _printUIntType tmp = value / i;
        _printUIntType rem = tmp % 10;
        if (rem > 0 || print)
        {
            print = true;
            func(INT_2_ASCII(rem), payload);
            charsWritten++;
        }
    }

    return charsWritten;
}

int32_t print_unsigned_hex(_PUT_CHAR_FP func, size_t payload,
                           _printUIntType value, bool upperCase)
{
    int32_t charsWritten = 0;

    int32_t asciiOffset = upperCase ? 'A' : 'a';

    bool print = false;
    for (_printUIntType i = _MAX_PRINT_UINT_HEX_VALUE; i > 0; i /= 16)
    {
        if (i == 1)
        {
            print = true;
        }

        _printUIntType tmp = value / i;
        _printUIntType rem = tmp % 16;
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
            else if ((*format == 'd') || (*format == 'i'))
            {
                _printIntType value = va_arg(vl, _printIntType);
                charsWritten += print_signed_int(func, payload, value);
            }
            else if (*format == 'u')
            {
                _printUIntType value = va_arg(vl, _printUIntType);
                charsWritten += print_unsigned_int(func, payload, value);
            }
            else if (*format == 'x')
            {
                _printUIntType value = va_arg(vl, _printUIntType);
                charsWritten += print_unsigned_hex(func, payload, value, false);
            }
            else if (*format == 'X')
            {
                _printUIntType value = va_arg(vl, _printUIntType);
                charsWritten += print_unsigned_hex(func, payload, value, true);
            }
            else if (*format == 'p')
            {
                _printUIntType value = va_arg(vl, _printUIntType);
                charsWritten += print_unsigned_hex(func, payload, value, false);
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
