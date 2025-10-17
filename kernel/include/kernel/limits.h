/* SPDX-License-Identifier: MIT */
#pragma once

// subset of defines found in linux/limits.h

#define NAME_MAX \
    60  ///< chars in a file name: 8+3 in DOS, 14 on xv6, 255 on linux etc.
#define PATH_MAX 128  ///< maximum file path name
