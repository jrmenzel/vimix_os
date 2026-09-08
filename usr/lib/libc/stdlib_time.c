/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <utime.h>

int utime(const char *path, const struct utimbuf *times)
{
    if (times == NULL)
    {
        return utimes(path, NULL);
    }

    struct timeval time_val[2];
    memset(time_val, 0, sizeof(times));
    time_val[1].tv_sec = times->modtime;

    return utimes(path, time_val);
}
