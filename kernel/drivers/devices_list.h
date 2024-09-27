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
void init_device(struct Supported_Device *dev);
