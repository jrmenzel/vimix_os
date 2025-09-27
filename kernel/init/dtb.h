/* SPDX-License-Identifier: MIT */

#pragma once

#include <arch/cpu.h>
#include <drivers/devices_list.h>
#include <kernel/kernel.h>
#include <libfdt.h>
#include <mm/vm.h>

/// @brief 64 bit values in the device tree are 32 bit aligned! Use this type
/// when casting void* from the device tree.
typedef uint64_t __attribute__((__aligned__(4))) dtb_aligned_uint64_t;

/// @brief 64 bit values in the device tree are 32 bit aligned! Use this type
/// when casting void* from the device tree.
typedef int64_t __attribute__((__aligned__(4))) dtb_aligned_int64_t;

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
/// @return dtb based timebase frequency (or fallback on error) in Hz.
uint64_t dtb_get_timebase(void *dtb);

ssize_t dtb_find_boot_console_in_dev_list(void *dtb,
                                          struct Devices_List *dev_list);

int32_t dtb_getprop32_with_fallback(const void *dtb, int node_offset,
                                    const char *name, int32_t fallback);

struct Device_Init_Parameters;

bool dtb_get_regs(void *dtb, int offset, struct Device_Init_Parameters *params);

bool dtb_get_reg(void *dtb, int offset, size_t *base, size_t *size);

int dtb_get_cpu_offset(void *dtb, size_t cpu_id, bool print_errors);

CPU_Features dtb_get_cpu_features(void *dtb, size_t cpu_id);
