/* SPDX-License-Identifier: MIT */

#pragma once

#include <drivers/devices_list.h>

dev_t bcm2711_pm_init(struct Device_Init_Parameters *init_param,
                      const char *name);
