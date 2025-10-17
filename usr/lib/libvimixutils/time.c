/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vimixutils/time.h>

/// @brief Get current time in milliseconds.
uint64_t get_time_ms()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    {
        fprintf(stderr, "clock_gettime failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}
