/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

// definitions for kobject

struct sysfs_attribute
{
    const char *name;
    mode_t mode;  ///< permission bits
};

struct kobject;
struct sysfs_ops
{
    ssize_t (*show)(struct kobject *kobj, size_t attribute_idx, char *buf,
                    size_t n);
    ssize_t (*store)(struct kobject *kobj, size_t attribute_idx,
                     const char *buf, size_t n);
};
