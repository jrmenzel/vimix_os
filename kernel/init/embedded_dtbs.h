/* SPDX-License-Identifier: MIT */
#pragma once

#include <init/system.h>
#include <kernel/kernel.h>

// This one optionally replaces the firmware provided DTB.
extern const unsigned char dtb_embedded[];

const unsigned char *get_embedded_dtb(enum Compatible_System compatible);
