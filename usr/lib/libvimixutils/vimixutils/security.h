/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

bool impersonate_user(const char *user, bool set_groups, bool change_cwd,
                      bool exec_shell);
