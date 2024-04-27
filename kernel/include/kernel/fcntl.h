/* SPDX-License-Identifier: MIT */
#pragma once

/// request file to be opened read-only
#define O_RDONLY 0x000

/// request file to be opened write-only
#define O_WRONLY 0x001

/// request file to be opened read-write
#define O_RDWR 0x002

/// create file if it didn't exist
#define O_CREAT 0x200
#define O_CREATE O_CREAT

/// if the file exists and is writeable, reset it to size 0
#define O_TRUNC 0x400

/// WARNING: not supported yet
#define O_APPEND 0x800
