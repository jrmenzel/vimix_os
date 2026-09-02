/* SPDX-License-Identifier: MIT */

#include <drivers/devices_list.h>
#include <kernel/kernel.h>

// The syscon device in RISC V qemu provides shutdown and reboot functionality.

/// @brief Init function, if never called the shutdown and reboot will panic if
/// this is the only way to shutdown.
/// @param init_parameters Only the memory address is relevant.
/// @param name Device name from the dtb file (if one driver supports multiple
/// devices)
/// @return Device number of successful init.
dev_t syscon_init(struct Device_Init_Parameters *init_parameters,
                  const char *name);
