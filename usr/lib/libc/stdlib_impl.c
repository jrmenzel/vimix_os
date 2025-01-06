/* SPDX-License-Identifier: MIT */

#include <kernel/limits.h>
#include <kernel/param.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "stdlib_impl.h"

int errno;

void (*at_exit_function[ATEXIT_MAX])(void) = {NULL};

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

long sysconf(int name)
{
    switch (name)
    {
        case _SC_PAGE_SIZE: return PAGE_SIZE;
        case _SC_ARG_MAX: return PAGE_SIZE;
        case _SC_OPEN_MAX: return MAX_FILES_PER_PROCESS;
        case _SC_ATEXIT_MAX: return ATEXIT_MAX;
    }

    return -1;
}

/// @brief Syscall for time().
/// @param tloc Where to store the returned time. the time is not returned
/// because it needs to be 64-bit on 32-bit systems as well.
/// @return -1 on error
ssize_t get_time(time_t *tloc);

time_t time(time_t *tloc)
{
    time_t time;
    get_time(&time);
    if (tloc != NULL)
    {
        *tloc = time;
    }

    return time;
}

#define SECONDS_PER_MINUTE (60)
#define MINUTES_PER_HOUR (60)
#define HOURS_PER_DAY (24)
#define SECONDS_PER_DAY (60 * 60 * 24)

bool is_leap_year(int year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

bool get_year(int day, int *year, int *day_in_year)
{
    const int year_len[] = {365, 366};

    int cur_year = 1970;
    bool is_leap;
    while (day > year_len[is_leap = is_leap_year(cur_year)])
    {
        day -= year_len[is_leap];
        cur_year++;
    }

    *year = cur_year;
    *day_in_year = day;

    return is_leap;
}

void get_month(int day, int *month, int *day_in_month, bool is_leap)
{
    int month_len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (is_leap)
    {
        month_len[1]++;
    }

    for (size_t m = 0; m < 12; ++m)
    {
        if (day < month_len[m])
        {
            *day_in_month = day + 1;
            *month = m;
            return;
        }
        day -= month_len[m];
    }

    // should not get reached
}

struct tm g_calendar_time;
struct tm *localtime(const time_t *timer)
{
    time_t time = *timer;
    int day = time / SECONDS_PER_DAY;
    int rem = time % SECONDS_PER_DAY;

    int sec = rem % SECONDS_PER_MINUTE;
    rem = rem / SECONDS_PER_MINUTE;

    int min = rem % MINUTES_PER_HOUR;
    rem = rem / MINUTES_PER_HOUR;

    int hour = rem % HOURS_PER_DAY;
    rem = rem / HOURS_PER_DAY;

    g_calendar_time.tm_sec = sec;
    g_calendar_time.tm_min = min;
    g_calendar_time.tm_hour = hour;

    int year;
    int day_in_year;
    bool is_leap_year = get_year(day, &year, &day_in_year);

    g_calendar_time.tm_year = year - 1970;
    g_calendar_time.tm_yday = day_in_year;

    int month;
    int day_in_month;
    get_month(day_in_year, &month, &day_in_month, is_leap_year);
    g_calendar_time.tm_mon = month;
    g_calendar_time.tm_mday = day_in_month;

    // tm_wday = 0 is Sunday
    // 1.1.1970 = day 0 = Thursday
    g_calendar_time.tm_wday = (day + 4) % 7;

    g_calendar_time.tm_isdst = 0;

    return &g_calendar_time;
}

int atexit(void (*function)(void))
{
    for (ssize_t i = 0; i < ATEXIT_MAX; ++i)
    {
        if (at_exit_function[i] == NULL)
        {
            at_exit_function[i] = function;
            return 0;
        }
    }
    return 1;
}
