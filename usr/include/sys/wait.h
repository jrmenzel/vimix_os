/* SPDX-License-Identifier: MIT */
#pragma once

#include <sys/types.h>

/// @brief Wait for a child process to exit and return its pid.
/// implemented in proc.c
/// @param wstatus address of an int to store wstatus into
/// @return -1 if this process has no children.
extern pid_t wait(int *wstatus);

/// returns true if the child terminated normally, that is, by
/// calling exit(3) or _exit(2), or by returning from main().
#define WIFEXITED(wstatus) (true)

/// returns the exit status of the child.
/// test WIFEXITED first!
#define WEXITSTATUS(wstatus) (wstatus)
