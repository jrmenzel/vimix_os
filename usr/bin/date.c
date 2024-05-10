/* SPDX-License-Identifier: MIT */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    time_t now = time(NULL);
    struct tm *cal_time = localtime(&now);

    // printf("Seconds since epoch: %ld\n", (int64_t)now);
    // printf("Day of the week: %d\n", cal_time->tm_wday);

    printf("%d.%d.%d %d:%d:%d\n", cal_time->tm_mday, cal_time->tm_mon + 1,
           1970 + cal_time->tm_year, cal_time->tm_hour, cal_time->tm_min,
           cal_time->tm_sec);

    return 0;
}
