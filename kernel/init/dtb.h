/* SPDX-License-Identifier: MIT */

#pragma once

#include <drivers/devices_list.h>
#include <kernel/kernel.h>
#include <kernel/vm.h>

/// @brief Updates dev_list, sets found flag and updates memory map
/// @param dtb Device Tree Binary pointer (provided by the boot loader)
void dtb_add_devices_to_dev_list(void *dtb, struct Device_Driver *driver_list,
                                 struct Devices_List *dev_list);

/// @brief Query the memory map, RAM size, ram disk location...
/// @param dtb Device Tree Binary pointer (provided by the boot loader)
/// @param memory_map Map to fill
void dtb_get_memory(void *dtb, struct Minimal_Memory_Map *memory_map);

/// @brief Returns the timebase frequency used by the timer.
/// @param dtb Device Tree Binary pointer (provided by the boot loader)
/// @return 0 on error or timebase frequency otherwise. In Hz.
uint64_t dtb_get_timebase(void *dtb);

ssize_t dtb_add_boot_console_to_dev_list(void *dtb,
                                         struct Devices_List *dev_list);

int32_t dtb_getprop32_with_fallback(const void *dtb, int node_offset,
                                    const char *name, int32_t fallback);
