/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/container_of.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>
#include <kernel/stdatomic.h>

struct sys_kernel
{
    struct kobject kobj;

    atomic_uint app_crash_verbosity;
};

#define sys_kernel_from_kobject(kobj_ptr) \
    container_of(kobj_ptr, struct sys_kernel, kobj)

syserr_t sys_kernel_init(struct kobject *parent);

void sys_kernel_release(struct kobject *kobj);

/// @brief How verbose an application crash should be reported.
/// 0 = no output
/// 1 = basic output
/// 2 = full output with register dump and call stack
/// @return Set value.
uint32_t sys_kernel_get_app_crash_verbosity();

ssize_t sys_kernel_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx,
                                  char *buf, size_t n);

ssize_t sys_kernel_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                                   const char *buf, size_t n);
