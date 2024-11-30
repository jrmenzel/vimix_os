/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

struct Device_Memory_Map
{
    size_t mem_start;
    size_t mem_size;
    int32_t interrupt;
};

typedef dev_t (*init_func_p)(struct Device_Memory_Map *);

struct Supported_Device
{
    const char *dtb_name;
    bool found;
    init_func_p init_func;
    dev_t dev_num;
    bool map_memory;
    struct Device_Memory_Map mapping;
};

struct Devices_List
{
    struct Supported_Device *dev;
    size_t dev_array_length;
};

struct Devices_List *get_devices_list();

void print_found_devices();
ssize_t get_first_virtio(struct Devices_List *dev_list);

/// @brief Returns the index of the diece. Assumes a device only exists once.
/// @param dev_list
/// @param name
/// @return Index or -1 on error
ssize_t get_device_index(struct Devices_List *dev_list, const char *name);

void init_device(struct Supported_Device *dev);
