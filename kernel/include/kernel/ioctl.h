/* SPDX-License-Identifier: MIT */
#pragma once

/// @brief Control of stream devices syscall
/// @param fd file descriptor of device
/// @param request request ID
/// @param ... Normally a pointer to a struct with parameters
/// @return request dependend
int ioctl(int fd, unsigned long request, ...);

#define TCGETA 0x5405
#define TCSETA 0x5406
#define TIOCGWINSZ 0x5413
