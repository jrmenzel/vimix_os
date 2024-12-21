/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>

/// @brief Adds itself to the devices list.
dev_t dev_null_init(struct Device_Init_Parameters *param, const char *name);