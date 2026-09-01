/* SPDX-License-Identifier: MIT */
#pragma once

// Driver for the starfive,jh7110-temp temperature sensor.
// based on a xv6 driver from Luc Videau (https://github.com/GerfautGE/xv6-mars)

#include <drivers/devices_list.h>

/// @brief Adds itself to the devices list.
dev_t jh7110_temp_init(struct Device_Init_Parameters *init_parameters,
                       const char *name);
