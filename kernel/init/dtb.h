/* SPDX-License-Identifier: MIT */

#pragma once

#include <drivers/devices_list.h>
#include <kernel/kernel.h>
#include <kernel/vm.h>

/// @brief Updates dev_list, sets found flag and updates memory map
/// @param dtb Device Tree Binary pointer (provided by the boot loader)
void dtb_get_devices(void *dtb, struct Devices_List *dev_list);

/// @brief Query the memory map, RAM size, ram disk location...
/// @param dtb Device Tree Binary pointer (provided by the boot loader)
/// @param memory_map Map to fill
void dtb_get_memory(void *dtb, struct Minimal_Memory_Map *memory_map);
