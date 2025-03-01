/* SPDX-License-Identifier: MIT */

#include <drivers/devices_list.h>
#include <kernel/kernel.h>

/// @brief Init function
/// @param init_parameters Only the memory address is relevant.
/// @param name Device name from the dtb file (if one driver supports multiple
/// devices)
/// @return Device number of successful init.
dev_t htif_init(struct Device_Init_Parameters *init_parameters,
                const char *name);

void htif_putc(int32_t c);

void htif_console_poll_input();
