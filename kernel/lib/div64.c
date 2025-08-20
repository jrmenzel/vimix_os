/* SPDX-License-Identifier: MIT */
#include <kernel/types.h>

// 64-bit division to be called by the compiler when generating 64-bit math on a
// 32-bit target CPU.

/// @brief unsigned 64-bit division on 32-bit platforms
/// See https://en.wikipedia.org/wiki/Division_algorithm
long long div_u64(unsigned long long n, unsigned long long d, long long *r)
{
    long long quot = 0;
    long long rem = 0;

    int N = 64;  // bits in n
    for (ssize_t i = N - 1; i >= 0; --i)
    {
        rem = rem << 1;
        rem = rem | ((n >> i) & 1);  // set rem bit 0 to n bit i

        if (rem >= d)
        {
            rem = rem - d;
            quot = quot | (1ll << (long long)i);  // set bit i
        }
    }

    *r = rem;
    return quot;
}

/// @brief 64-bit division on 32-bit platforms
/// See https://en.wikipedia.org/wiki/Division_algorithm
/// @param n
/// @param d
/// @param r remainder
/// @return n/d
long long div_64(long long n, long long d, long long *r)
{
    if (d == 0)
    {
        // division by zero, try to trigger a real one for the exception:
        return (size_t)n / (size_t)d;
    }

    if (d < 0)
    {
        long long quot = div_64(n, -d, r);
        return -quot;
    }

    if (n < 0)
    {
        long long quot = div_64(-n, d, r);
        if (*r == 0)
        {
            return -quot;
        }
        else
        {
            *r = d - *r;
            return -quot - 1;
        }
    }

    // n > 0 && d > 0 -> call unsigned div
    return div_u64(n, d, r);
}

long long __divdi3(long long n, long long d)
{
    long long r;
    return div_64(n, d, &r);
}

long long __moddi3(long long n, long long d)
{
    long long r;
    div_64(n, d, &r);
    return r;
}

unsigned long long __udivdi3(unsigned long long n, unsigned long long d)
{
    long long r;
    return div_u64(n, d, &r);
}

unsigned long long __umoddi3(unsigned long long n, unsigned long long d)
{
    long long r;
    div_u64(n, d, &r);
    return (unsigned long long)r;
}
