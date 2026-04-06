/* SPDX-License-Identifier: MIT */
#pragma once

#include <fs/dentry.h>
#include <kernel/container_of.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>
#include <kernel/spinlock.h>

/// @brief Dentry cache structure holding the known subset of the file system
/// tree. Currentry known and unknown entries are in a tree structure starting
/// from root. Entries with reference count 0 are also kept in a last recently
/// used (LRU) list for easy eviction.
/// If a file gets created or deleted while others hold references to the
/// previous dentry, the old dentry is moved to the unlinked list. Their parent
/// is set to NULL.
struct dentry_cache
{
    struct kobject kobj;
    struct dentry *root;
    struct spinlock list_lock;
    struct list_head lru_list;
    struct list_head unlinked_list;
    struct dentry *unlinked_root;
    size_t lru_size;
    size_t max_lru_size;
};

#define dentry_cache_from_kobj(kobj_ptr) \
    container_of(kobj_ptr, struct dentry_cache, kobj)

extern struct dentry_cache g_dentry_cache;

/// @brief Init the dentry cache, called once when root gets mounted.
/// Creates the first dentry object pointed to by g_dentry_cache with ref
/// count 1.
/// @param root_ip Inode of root.
/// @return The root dentry.
struct dentry *dentry_cache_init(struct inode *root_ip);

/// @brief Returns the root dentry of the dentry cache.
/// @return The root dentry, reference count is increased by 1.
struct dentry *dentry_cache_get_root();

/// @brief Look up a dentry by name in the given parent dentry's children.
/// The returned dentry has its reference count increased by 1.
/// @param parent The parent dentry.
/// @param name The name of the child dentry to look for.
/// @return The found dentry with increased reference count, or NULL if not
/// found.
struct dentry *dentry_cache_lookup(struct dentry *parent, const char *name);

/// @brief Same as dentry_cache_lookup but requires the parent dentry to be
/// locked already.
/// @param parent The parent dentry, must be locked.
/// @param name The name of the child dentry to look for.
/// @return The found dentry with increased reference count, or NULL if not
/// found.
struct dentry *dentry_cache_lookup_locked(struct dentry *parent,
                                          const char *name);

/// @brief Add a dentry to the dentry cache under the given parent with the
/// given name and inode. If the dentry already exists by name, returns the
/// existing one with increased reference count.
/// @param parent The parent dentry.
/// @param name The name of the child dentry to add.
/// @param ip The inode the new dentry refers to, can be NULL for invalid
/// dentry.
/// @return The added or existing dentry with increased reference count. NULL on
/// out of memory.
struct dentry *dentry_cache_add(struct dentry *parent, const char *name,
                                struct inode *ip);

void dentry_cache_add_to_unlinked(struct dentry *dp);

void dentry_cache_remove_from_unlinked(struct dentry *dp);

syserr_t dentry_cache_set_max_lru(struct dentry_cache *dcache,
                                  size_t max_lru_size);

syserr_t dentry_cache_clear_lru(struct dentry_cache *dcache);

static inline bool dentry_is_unlinked(struct dentry *dp)
{
    return dp->parent == g_dentry_cache.unlinked_root;
}

/// @brief Prints the dentry cache to the console. Does not lock the dentries
/// and is thus not free of races.
void debug_print_dentry_cache();

void debug_print_path(struct dentry *dentry);
