/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

#define DEVICE_MAX_MEM_MAPS (4)
#define DEVICE_MAX_CLOCKS (4)
struct Memory_Mapped_Registers
{
    size_t start_pa;
    size_t start_va;
    size_t size;
    const char *name;  ///< optional, can be NULL
};

struct Device_Init_Parameters
{
    struct Memory_Mapped_Registers mem[DEVICE_MAX_MEM_MAPS];
    bool mmu_map_memory;
    int32_t reg_io_width;
    int32_t reg_shift;
    int32_t interrupt;
    const void *dtb;   ///< device tree pointer
    int dev_offset;    ///< in the dtb file
    uint32_t phandle;  ///< 0 if device has no phandle in the device tree
    uint32_t interrupt_parent_phandle;  ///< or 0 if not present
    uint32_t clock_phandles[DEVICE_MAX_CLOCKS];
};

typedef dev_t (*init_func_p)(struct Device_Init_Parameters *, const char *name);

enum Driver_Type
{
    VIRTUAL,  ///< always present, e.g. /dev/null
    PHYSICAL  ///< found in the device tree
};

struct Driver
{
    const char *dtb_name;
    init_func_p init_func;

    enum Driver_Type type;
};

#define for_each_driver(driver_var)                        \
    for (struct Driver *driver_var = g_driver_list.driver; \
         driver_var < g_driver_list.driver_end; ++driver_var)

// helper macros to generate unique names in REGISTER_ macros
#define COMBINE1(X, Y) X##Y
#define COMBINE(X, Y) COMBINE1(X, Y)

#define REGISTER_DRIVER_WITH_TYPE(dtb_name, init_func, type)              \
    __attribute__((used, section(".driver_list"))) struct Driver COMBINE( \
        driver_##init_func, __LINE__) = {dtb_name, init_func, type};

///< Register a driver which gets initialized if the dtb_name was found in the
///< device tree.
#define REGISTER_DRIVER(dtb_name, init_func) \
    REGISTER_DRIVER_WITH_TYPE(dtb_name, init_func, PHYSICAL)

///< Register a virtual driver which is always initialized, e.g. /dev/null.
#define REGISTER_VIRTUAL_DRIVER(dtb_name, init_func) \
    REGISTER_DRIVER_WITH_TYPE(dtb_name, init_func, VIRTUAL)

struct Driver_List
{
    struct Driver *driver;
    struct Driver *driver_end;
};

extern struct Driver_List g_driver_list;

/// @brief Call during boot to init the global list of all available drivers.
void driver_list_init();

void debug_print_driver_list();
