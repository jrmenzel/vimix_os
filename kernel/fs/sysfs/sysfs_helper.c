/* SPDX-License-Identifier: MIT */

#include <fs/sysfs/sysfs_helper.h>

int32_t store_param_to_int(const char *buf, size_t n, bool *ok)
{
    // modified atoi
    *ok = false;

    if (n == 0)
    {
        return 0;
    }

    char sign = '+';
    if (*buf == '-' || *buf == '+')
    {
        sign = *buf++;
        n--;
    }

    int32_t value = 0;
    while ((n > 0) && ('0' <= *buf) && (*buf <= '9'))
    {
        *ok = true;
        value = value * 10 + *buf++ - '0';
        n--;
    }

    if (sign == '-')
    {
        value = value * (-1);
    }
    return value;
}
