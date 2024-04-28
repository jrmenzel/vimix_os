/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/signal.h>
#include <sys/types.h>

/// @brief Sends a signal to a process
/// @param pid The PID of the process
/// @param sig The signal
/// @return -1 on failure, 0 otherwise
extern int32_t kill(pid_t pid, int sig);
