/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/reboot.h>
#include <sys/types.h>

/// @brief Reboots or halts the system
/// @param cmd Command from kernel/reboot.h:
/// VIMIX_REBOOT_CMD_POWER_OFF or VIMIX_REBOOT_CMD_RESTART
extern ssize_t reboot(int32_t cmd);