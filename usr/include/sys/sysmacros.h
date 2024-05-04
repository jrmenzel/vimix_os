/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/major.h>

#define major(x) MAJOR(x)
#define minor(x) MINOR(x)
#define makedev(x, y) MKDEV(x, y)
