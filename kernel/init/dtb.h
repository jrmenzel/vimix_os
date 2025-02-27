/* SPDX-License-Identifier: MIT */

#pragma once

#include <drivers/devices_list.h>
#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <libfdt.h>

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

ssize_t dtb_add_boot_console_to_dev_list(void *dtb,
                                         struct Devices_List *dev_list);

int32_t dtb_getprop32_with_fallback(const void *dtb, int node_offset,
                                    const char *name, int32_t fallback);

/// @brief Returns #size-cells value: number of 32 bit values per size in a reg
/// field.
/// @param dtb Device tree
uint32_t dtb_get_size_cells(void *dtb);

/// @brief Returns #address-cells value: number of 32 bit values per address in
/// a reg field.
/// @param dtb Device tree
uint32_t dtb_get_address_cells(void *dtb);

bool dtb_get_reg(void *dtb, int offset, size_t *base, size_t *size);

typedef uint32_t CPU_Features;
#define RV_SV32_SUPPORTED 0x01
#define RV_SV39_SUPPORTED 0x02
#define RV_SV48_SUPPORTED 0x04
#define RV_SV57_SUPPORTED 0x08
#define RV_EXT_FLOAT 0x10
#define RV_EXT_DOUBLE 0x20
#define RV_EXT_SSTC 0x40

CPU_Features dtb_get_cpu_features(void *dtb, size_t cpu_id);
