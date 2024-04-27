/* SPDX-License-Identifier: MIT */

#include <stdlib.h>
#include <unistd.h>

int abs(int j) { return (j < 0) ? -j : j; }

long int labs(long int j) { return (j < 0) ? -j : j; }

long long int llabs(long long int j) { return (j < 0) ? -j : j; }

// convert a string to an integer
int32_t atoi(const char *string)
{
    char sign = '+';
    if (*string == '-' || *string == '+')
    {
        sign = *string++;
    }

    int32_t n = 0;
    while ('0' <= *string && *string <= '9')
    {
        n = n * 10 + *string++ - '0';
    }

    if (sign == '-')
    {
        n = n * (-1);
    }
    return n;
}
