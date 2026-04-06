/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/container_of.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/kref.h>
#include <kernel/list.h>
#include <kernel/spinlock.h>

/// @brief A directory entry (dentry) represents a file inside a directory.
/// It holds the name and the inode of the file (or inode == NULL to indicate a
/// file does not exist).
/// Dentries are reference counted and form an in memory tree structure
/// mirroring a subset of the file system tree structure.
/// Currently unused dentries (ref count == 0) are kept in the tree for caching
/// but are *also* added to the last recently used (LRU) list for easy eviction.
struct dentry
{
    struct kref ref;       ///< reference count
    struct spinlock lock;  ///< protects everything below here
    struct inode *ip;      ///< inode this dentry refers to
    const char *name;      ///< name of the file

    struct dentry *parent;          ///< parent dentry, NULL for root dentry
    struct list_head child_list;    ///< list of child dentries
    struct list_head sibling_list;  ///< list head for siblings

    struct list_head lru_list;  ///< for last recently used list
};

#define dentry_from_child_list(ptr) \
    container_of((ptr), struct dentry, sibling_list)

#define dentry_from_lru_list(ptr) container_of((ptr), struct dentry, lru_list)

/// @brief Allocates an invalid dentry (no name, no inode).
/// The reference count is initialized to 1.
/// @return A fresh dentry or NULL on error.
struct dentry *dentry_alloc();

/// @brief Allocates and initializes an orphan dentry (no parent) with the given
/// name. Caller must manually delete it or register a parent via
/// dentry_register_with_parent().
struct dentry *dentry_alloc_init_orphan(const char *name, struct inode *ip);

/// @brief Register an orphan dentry as a child of the given parent. Parent must
/// be locked by caller.
/// @param parent The parent dentry.
/// @param child The child dentry to register.
void dentry_register_with_parent(struct dentry *parent, struct dentry *child);

/// @brief Unregister a child dentry from its parent. Parent must be locked by
/// caller.
/// @param child The child dentry to unregister.
void dentry_unregister_from_parent(struct dentry *child);

/// @brief Allocates and initializes a dentry with the given parent and name.
/// The reference count is initialized to 1.
/// The dentry is registered as a child of the given parent.
/// @param parent The parent dentry.
/// @param name The name of the new dentry.
/// @return The allocated dentry or NULL on error.
struct dentry *dentry_alloc_init(struct dentry *parent, const char *name,
                                 struct inode *ip);

/// @brief Switching dentries in the tree. Used by mount/umount.
/// @param old_dp Old dentry to switch out.
/// @param new_dp New dentry to switch in.
void dentry_switch_children(struct dentry *old_dp, struct dentry *new_dp);

/// @brief Frees a dentry. The reference count must be 0.
/// @param dp The dentry to free.
void dentry_free(struct dentry *dp);

/// @brief Increase reference count of dentry and return it.
/// @param dp Dentry to get.
/// @return The same dentry with increased reference count.
struct dentry *dentry_get(struct dentry *dp);

/// @brief Decrease reference count of dentry. If it reaches 0,
/// @param dp The dentry to put.
void dentry_put(struct dentry *dp);

#define DEBUG_CHECK_DENTRY(dp)                                                \
    DEBUG_EXTRA_PANIC((dp) != NULL, "DEBUG_CHECK_DENTRY: dp is NULL");        \
    DEBUG_EXTRA_PANIC(kref_read(&(dp)->ref) > 0,                              \
                      "DEBUG_CHECK_DENTRY: trying to (un)lock a dentry with " \
                      "invalid reference count");

static inline void dentry_lock(struct dentry *dp)
{
    DEBUG_CHECK_DENTRY(dp);

    spin_lock(&dp->lock);
}

static inline bool dentry_trylock(struct dentry *dp)
{
    DEBUG_CHECK_DENTRY(dp);

    return spin_trylock(&dp->lock);
}

static inline void dentry_unlock(struct dentry *dp)
{
    DEBUG_CHECK_DENTRY(dp);

    spin_unlock(&dp->lock);
}

static inline void dentry_unlock_put(struct dentry *dp)
{
    DEBUG_CHECK_DENTRY(dp);

    spin_unlock(&dp->lock);
    dentry_put(dp);
}

static inline bool dentry_is_valid(struct dentry *dp)
{
    DEBUG_CHECK_DENTRY(dp);

    return (dp->ip != NULL);
}

static inline bool dentry_is_invalid(struct dentry *dp)
{
    DEBUG_CHECK_DENTRY(dp);

    return (dp->ip == NULL);
}
