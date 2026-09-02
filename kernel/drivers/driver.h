/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <drivers/mmio_access.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/stdatomic.h>
#include <mm/kalloc.h>

#define DRIVER_CHECK_INIT_PARAMS(init_parameters)            \
    DEBUG_EXTRA_PANIC(init_parameters != NULL,               \
                      "Driver init parameters are NULL");    \
    DEBUG_EXTRA_PANIC(init_parameters->mem[0].start_pa != 0, \
                      "Driver memory-mapped location is 0"); \
    DEBUG_EXTRA_PANIC(init_parameters->mem[0].size != 0,     \
                      "Driver memory-mapped size is 0");

#define DRIVER_CHECK_INIT_PARAMS_DTB(init_parameters) \
    DRIVER_CHECK_INIT_PARAMS(init_parameters);        \
    DEBUG_EXTRA_PANIC(init_parameters->dtb != NULL,   \
                      "Driver init parameter DTB is NULL");

#define DRIVER_CHECK_INIT_PARAMS_DTB_ONLY(init_parameters) \
    DEBUG_EXTRA_PANIC(init_parameters != NULL,             \
                      "Driver init parameters are NULL");  \
    DEBUG_EXTRA_PANIC(init_parameters->dtb != NULL,        \
                      "Driver init parameter DTB is NULL");
