/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

struct Device_Init_Parameters
{
    size_t mem_start;
    size_t mem_size;
    bool mmu_map_memory;
    int32_t reg_io_width;
    int32_t reg_shift;
    int32_t interrupt;
};

typedef dev_t (*init_func_p)(struct Device_Init_Parameters *);

void clear_init_parameters(struct Device_Init_Parameters *param);

/// @brief A found device which might not have been initialized yet.
struct Found_Device
{
    const char *dtb_name;   ///< Points to the name in the Device Tree
    init_func_p init_func;  ///< Drivers init function
    dev_t dev_num;  ///< Device number is set when init_func is called -> 0
                    ///< means uninitialized
    struct Device_Init_Parameters init_parameters;
};

struct Devices_List
{
    struct Found_Device *dev;
    size_t dev_array_length;
};

struct Devices_List *get_devices_list();

struct Device_Driver
{
    const char *dtb_name;
    init_func_p init_func;
};

/// @brief Return drivers which can be used for the console
struct Device_Driver *get_console_drivers();

/// @brief A list of all drivers expected and supported in the device tree
struct Device_Driver *get_generell_drivers();

/// @brief Interrupt handlers.
struct Device_Driver *get_interrupt_drivers();

/// @brief Init one individual device.
/// @param dev Uninitialized device.
void init_device(struct Found_Device *dev);

/// @brief Returns the index of the initialized device with the lowest memory
/// init_parameters with a given name. Good to find the first qemu disk.
/// @param dev_list Device list.
/// @param name Name of the device to find.
/// @return Index or -1 on error.
ssize_t dev_list_get_first_device_index(struct Devices_List *dev_list,
                                        const char *name);

/// @brief Returns the index of the device. Assumes a device only exists once.
/// @param dev_list Device list.
/// @param name Name of the device to find.
/// @return Index or -1 on error.
ssize_t dev_list_get_device_index(struct Devices_List *dev_list,
                                  const char *name);

/// @brief Get a free device, will be set as used interally.
/// @param dev_list Device list.
/// @param index will return the index in the device list if not NULL.
/// @return Device to fill with info or NULL.
struct Found_Device *dev_list_get_free_device(struct Devices_List *dev_list,
                                              ssize_t *index);

/// @brief Init every device in the list. Already initialized devices will be
/// skipped.
/// @param dev_list Device list.
void dev_list_init_all_devices(struct Devices_List *dev_list);

/// @brief Add a trivial device to the devices list.
/// @param dev_list Device list a new device will be added to.
/// @param name Name of the device, e.g. dtb name.
/// @param init_func Function to call to init the device later.
/// @return Index or -1 on error.
ssize_t dev_list_add(struct Devices_List *dev_list, const char *name,
                     init_func_p init_func);

/// @brief Add a memory mapped device to the devices list.
/// @param dev_list Device list a new device will be added to.
/// @param name Name of the device, e.g. dtb name.
/// @param init_func Function to call to init the device later.
/// @param init_parameters Additional parameters like the memory map.
/// @return Index or -1 on error.
ssize_t dev_list_add_with_parameters(
    struct Devices_List *dev_list, const char *name, init_func_p init_func,
    struct Device_Init_Parameters init_parameters);

ssize_t dev_list_add_from_dtb(struct Devices_List *dev_list, void *dtb,
                              const char *device_name, int device_offset,
                              struct Device_Driver *driver);

void debug_dev_list_print(struct Devices_List *dev_list);
