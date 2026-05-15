/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/driver_list.h>
#include <kernel/kernel.h>
#include <kernel/list.h>

void clear_init_parameters(struct Device_Init_Parameters *param);

/// @brief A found device which might not have been initialized yet.
struct Found_Device
{
    struct Driver *driver;  ///< name in the Device Tree & drivers init function
    dev_t dev_num;  ///< Device number is set when init_func is called -> 0
                    ///< means uninitialized
    struct Device_Init_Parameters init_parameters;

    struct list_head list;  ///< for devices list
};

struct Found_Device *found_device_alloc_init(
    struct Driver *driver, struct Device_Init_Parameters init_parameters);

struct Devices_List
{
    struct list_head devices;  ///< devices list
};

#define found_device_from_devices_list(ptr) \
    container_of(ptr, struct Found_Device, list)

/// @brief Init the global devices list.
void dev_list_init();

/// @brief Get the global devices list.
struct Devices_List *get_devices_list();

/// @brief Adds all devices which are always present (e.g. /dev/null) to the
/// devices list.
/// @param dev_list Device list to which the virtual devices will be added.
void dev_list_add_virtual_devices(struct Devices_List *dev_list);

/// @brief Init one individual device.
/// @param dev_list devices list (to also find dependencies)
/// @param dev pointer to the device info to initialize
/// @return valid device number if the device was initialized
dev_t init_device(struct Devices_List *dev_list, struct Found_Device *dev);

/// @brief Init one individual device with a certain name. Used by devices to
/// init dependencies that are not found in the device tree.
/// @param dev_list devices list (to also find dependencies)
/// @param dtb_name name of the device to initialize
/// @return valid device number if the device was initialized
dev_t init_device_by_name(struct Devices_List *dev_list, const char *dtb_name);

/// @brief Returns the index of the initialized device with the lowest memory
/// init_parameters with a given name. Good to find the first qemu disk.
/// @param dev_list Device list.
/// @param name Name of the device to find.
/// @return Index or -1 on error.
struct Found_Device *dev_list_get_first_device(struct Devices_List *dev_list,
                                               const char *name);

/// @brief Init every device in the list. Already initialized devices will be
/// skipped.
/// @param dev_list Device list.
void dev_list_init_all_devices(struct Devices_List *dev_list);

/// @brief Add a memory mapped device to the devices list.
/// @param dev_list Device list a new device will be added to.
/// @param driver Device driver struct which holds the name, init func etc.
/// @param init_parameters Additional parameters for this device instance like
/// the memory map.
/// @return Index or -1 on error.
ssize_t dev_list_add_with_parameters(
    struct Devices_List *dev_list, struct Driver *driver,
    struct Device_Init_Parameters init_parameters);

/// @brief Add a device from the device tree to the devices list.
/// @param dev_list Device list a new device will be added to.
/// @param dtb Device tree pointer.
/// @param device_name Name of the device in the device tree.
/// @param device_offset Offset of the device in the device tree.
/// @param driver Device driver struct which holds the name, init func etc.
/// @return Index or -1 on error.
ssize_t dev_list_add_from_dtb(struct Devices_List *dev_list, const void *dtb,
                              const char *device_name, int device_offset,
                              struct Driver *driver);

void debug_dev_list_print(struct Devices_List *dev_list);
