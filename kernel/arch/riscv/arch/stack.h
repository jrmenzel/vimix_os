/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

#define return_address_from_frame_pointer(frame_pointer) \
    (frame_pointer - 1 * sizeof(size_t))

#define next_fp_addr_from_frame_pointer(frame_pointer) \
    (frame_pointer - 2 * sizeof(size_t))
