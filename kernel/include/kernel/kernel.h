/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/param.h>
#include <kernel/types.h>

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
