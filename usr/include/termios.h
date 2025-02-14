/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/termios.h>

/// @brief Get termios parameters of fd
/// @param fd file descriptor
/// @param termios_p where the parameters get stored
/// @return ioctl return code
int tcgetattr(int fd, struct termios *termios_p);

/// @brief Sets terminal to a "raw" mode:
/// * input is avalable chat by char
/// * echoing is disabled
/// * no processing on input
/// @param termios_p
void cfmakeraw(struct termios *termios_p);

/// @brief Sets termios parameters
/// @param fd
/// @param optional_actions
/// @param termios_p the parameters to set
/// @return ioctl return code
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
