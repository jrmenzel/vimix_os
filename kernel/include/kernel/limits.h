/* SPDX-License-Identifier: MIT */
#pragma once

// subset of defines found in linux/limits.h

///< chars in a file name: 8+3 in DOS, 14 on xv6, 255 on linux etc.
#define NAME_MAX 60

///< maximum file path name
#define PATH_MAX 128

///< maximum number of supplementary group IDs a process may have
#define NGROUPS_MAX 16
