/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/container_of.h>
#include <kernel/kernel.h>
#include <kernel/kref.h>
#include <kernel/list.h>
#include <kernel/rwspinlock.h>

struct kobject;
struct kobj_type
{
    void (*release)(struct kobject *kobj);
    // const struct sysfs_ops *sysfs_ops;
};

/// @brief Represents a kernel object in a hierarchy.
/// Inspired by Linux kobject.
/// Every kobject has a name, a parent (except for the root kobject),
/// a list of children, a reference count and a kobj_type with type specific
/// function pointers (e.g. release() function)
struct kobject
{
    const char *name;                 ///< human readable name of the object
    struct kobject *parent;           ///< parent kobject, NULL for root kobject
    struct list_head children;        ///< list of child kobjects
    struct rwspinlock children_lock;  ///< protects the children list
    struct list_head siblings;  ///< node in parent's children list, protected
                                ///< by parents lock

    const struct kobj_type *ktype;  ///< object specific callbacks, can be NULL
    // struct sysfs_...
    struct kref ref_count;  ///< reference count for this object
};

#define kobject_from_child_list(ptr) container_of(ptr, struct kobject, siblings)

extern struct kobject g_kobjects_root;
extern struct kobject g_kobjects_proc;
extern struct kobject g_kobjects_dev;
extern struct kobject g_kobjects_fs;

extern const struct kobj_type default_kobj_ktype;

/// @brief Call early during boot. Initializes the root kobject "sys".
void init_kobject_root();

/// @brief Alloc and initializes a new dynamic kobject.
/// @return NULL on failure
struct kobject *kobject_create();

/// @brief Initializes a kobject. Call kobject_add() next to add it to the
/// kobject hierarchy.
/// @param kobj The kobject to init.
/// @param ktype The kobj_type, can be NULL.
void kobject_init(struct kobject *kobj, const struct kobj_type *ktype);

/// @brief Add kobject to the kobject hierarchy.
/// Increments the reference count of the kobject.
/// @param kobj The kobject to add.
/// @param parent The parent kobject.
/// @param fmt Name of the kobject, printf style format string.
/// Additional arguments depend on fmt.
/// @return True on success, false on failure.
bool kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt,
                 ...) __attribute__((format(printf, 3, 4)));

/// @brief Removes a kobject from the kobject hierarchy and decrements its
/// reference count. If the reference count reaches 0, the kobject gets freed.
/// @param kobj The kobject to delete.
void kobject_del(struct kobject *kobj);

/// @brief Increments the reference count of the kobject.
/// @param kobj The kobject.
static inline void kobject_get(struct kobject *kobj)
{
    DEBUG_EXTRA_ASSERT(kobj != NULL, "kobject_get() on NULL");
    kref_get(&kobj->ref_count);
}

/// @brief Decrements the reference count of the kobject. If the reference
/// count reaches 0, the kobject gets freed.
/// @param kobj The kobject.
void kobject_put(struct kobject *kobj);

/// @brief Debug function to print the kobject tree starting from the root
/// kobject "sys".
void debug_print_kobject_tree();
