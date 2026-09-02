/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>

// ARM PL031 RTC driver, found for example on the qemu virt board
// https://support.arm.com/documentation/ddi0224/c/Programmers-model/General-registers

// 32-bit time value in seconds
#define RTCDR 0x00

/// @brief Inits the RTC.
/// @param init_parameters MMIO parameters.
/// @param name Device name from the dtb file (if one driver supports multiple
/// devices)
/// @return Device number of successful init.
dev_t arm_pl031_init(struct Device_Init_Parameters *init_parameters,
                     const char *name);
