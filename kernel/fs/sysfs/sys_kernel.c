/* SPDX-License-Identifier: MIT */

#include <fs/sysfs/sys_kernel.h>
#include <fs/sysfs/sysfs_helper.h>
#include <mm/kalloc.h>

struct sys_kernel *g_sys_kernel = NULL;

enum SYS_KERNEL_ATTRIBUTE_INDEX
{
    SK_APP_CRASH_VERBOSITY = 0,
};

struct sysfs_attribute sys_kernel_attributes[] = {
    [SK_APP_CRASH_VERBOSITY] = {.name = "app_crash_v", .mode = 0644}};

struct sysfs_ops sys_kernel_sysfs_ops = {
    .show = sys_kernel_sysfs_ops_show,
    .store = sys_kernel_sysfs_ops_store,
};

const struct kobj_type sys_kernel_kobj_ktype = {
    .release = sys_kernel_release,
    .sysfs_ops = &sys_kernel_sysfs_ops,
    .attribute = sys_kernel_attributes,
    .n_attributes =
        sizeof(sys_kernel_attributes) / sizeof(sys_kernel_attributes[0])};

syserr_t sys_kernel_init(struct kobject *parent)
{
    DEBUG_EXTRA_PANIC(g_sys_kernel == NULL,
                      "sys_kernel_init: sys_kernel already initialized");

    g_sys_kernel = kmalloc(sizeof(struct sys_kernel), ALLOC_FLAG_ZERO_MEMORY);
    if (g_sys_kernel == NULL)
    {
        return -ENOMEM;
    }

    atomic_init(&g_sys_kernel->app_crash_verbosity, 2);

    kobject_init(&g_sys_kernel->kobj, &sys_kernel_kobj_ktype);
    if (!kobject_add(&g_sys_kernel->kobj, parent, "kernel"))
    {
        kfree(g_sys_kernel);
        g_sys_kernel = NULL;
        return -ENOMEM;
    }
    kobject_put(&g_sys_kernel->kobj);

    return 0;
}

void sys_kernel_release(struct kobject *kobj)
{
    struct sys_kernel *sys_kern = sys_kernel_from_kobject(kobj);
    kfree(sys_kern);
}

uint32_t sys_kernel_get_app_crash_verbosity()
{
    if (g_sys_kernel == NULL)
    {
        return 2;  // default verbosity
    }
    return atomic_load(&g_sys_kernel->app_crash_verbosity);
}

syserr_t sys_kernel_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx,
                                   char *buf, size_t n)
{
    struct sys_kernel *sys_kern = sys_kernel_from_kobject(kobj);

    syserr_t ret = 0;
    switch (attribute_idx)
    {
        case SK_APP_CRASH_VERBOSITY:
            ret = snprintf(buf, n, "%d\n",
                           atomic_load(&sys_kern->app_crash_verbosity));
            break;
        default: ret = -ENOENT; break;
    }

    if (ret == -1)
    {
        // snprintf error
        ret = -EOTHER;
    }

    return ret;
}

syserr_t sys_kernel_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                                    const char *buf, size_t n)
{
    struct sys_kernel *sys_kern = sys_kernel_from_kobject(kobj);

    bool ok;
    int32_t value = store_param_to_int(buf, n, &ok);
    if (!ok)
    {
        return -EINVAL;
    }

    syserr_t ret = -EINVAL;
    switch (attribute_idx)
    {
        case SK_APP_CRASH_VERBOSITY:
        {
            if (value < 0 || value > 2)
            {
                break;
            }
            atomic_store(&sys_kern->app_crash_verbosity, (atomic_uint)value);
            ret = n;
        }
        break;
        default: break;
    }

    return ret;
}
