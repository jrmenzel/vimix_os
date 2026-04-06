/* SPDX-License-Identifier: MIT */

#include <fs/dentry.h>
#include <fs/dentry_cache.h>
#include <kernel/string.h>
#include <mm/kalloc.h>

//
// dentry management
//

// from dentry_cache (only needed here)
void dentry_cache_move_to_lru(struct dentry *dp);

/// @brief Same as dentry_cache_lookup but requires parent to be locked.
struct dentry *dentry_cache_lookup_locked(struct dentry *parent,
                                          const char *name);

struct dentry *dentry_alloc()
{
    struct dentry *new_dentry = kmalloc(sizeof(struct dentry), 0);
    if (new_dentry == NULL)
    {
        return NULL;
    }

    // init fields
    kref_init(&new_dentry->ref);
    spin_lock_init(&new_dentry->lock, "dentry_lock");
    new_dentry->ip = NULL;
    new_dentry->name = NULL;
    new_dentry->parent = NULL;
    list_init(&new_dentry->child_list);
    list_init(&new_dentry->sibling_list);
    list_init(&new_dentry->lru_list);

    return new_dentry;
}

struct dentry *dentry_alloc_init_orphan(const char *name, struct inode *ip)
{
    struct dentry *new_dentry = dentry_alloc();
    if (new_dentry == NULL)
    {
        return NULL;
    }

    // copy name
    size_t str_len = strlen(name);
    DEBUG_EXTRA_PANIC(str_len <= NAME_MAX,
                      "dentry_alloc_init_orphan: name too long");

    new_dentry->name = kmalloc(str_len + 1, 0);
    if (new_dentry->name == NULL)
    {
        kfree(new_dentry);
        return NULL;
    }
    memcpy((char *)new_dentry->name, name, str_len + 1);

    if (ip != NULL)
    {
        new_dentry->ip = inode_get(ip);
    }
    else
    {
        new_dentry->ip = NULL;
    }

    return new_dentry;
}

void dentry_register_with_parent(struct dentry *parent, struct dentry *child)
{
    DEBUG_EXTRA_PANIC(spin_lock_is_held_by_this_cpu(&parent->lock),
                      "dentry_register_with_parent: parent lock not held");

    child->parent = dentry_get(parent);
    list_add(&child->sibling_list, &parent->child_list);
}

void dentry_unregister_from_parent(struct dentry *child)
{
    DEBUG_EXTRA_PANIC(child != NULL,
                      "dentry_unregister_from_parent: child is NULL");
    if (child->parent == NULL)
    {
        return;
    }
    DEBUG_EXTRA_PANIC(spin_lock_is_held_by_this_cpu(&child->parent->lock),
                      "dentry_unregister_from_parent: parent lock not held");

    list_del(&child->sibling_list);
    dentry_put(child->parent);  // drop parent's reference to child
    child->parent = NULL;
}

struct dentry *dentry_alloc_init(struct dentry *parent, const char *name,
                                 struct inode *ip)
{
    struct dentry *new_dentry = dentry_alloc_init_orphan(name, ip);
    if (new_dentry == NULL)
    {
        return NULL;
    }

    dentry_lock(parent);
    dentry_register_with_parent(parent, new_dentry);
    dentry_unlock(parent);

    return new_dentry;
}

void dentry_switch_children(struct dentry *old_dp, struct dentry *new_dp)
{
    bool locked = false;
    do
    {
        while (true)
        {
            if (dentry_trylock(old_dp))
            {
                if (dentry_trylock(new_dp))
                {
                    locked = true;
                    break;
                }
                dentry_unlock(old_dp);
            }
        }
        // now both old_dp and new_dp are locked
        DEBUG_EXTRA_ASSERT(old_dp->parent != NULL,
                           "dentry_switch_children: old_dp has no parent");
        if (dentry_trylock(old_dp->parent) == false)
        {
            dentry_unlock(new_dp);
            dentry_unlock(old_dp);
            continue;
        }

    } while (locked == false);

    list_del(&old_dp->sibling_list);
    list_add(&new_dp->sibling_list, &old_dp->parent->child_list);
    // skip get/put ownership transfer, "move" ownership
    new_dp->parent = old_dp->parent;
    old_dp->parent = NULL;

    dentry_unlock(new_dp->parent);
    dentry_unlock(new_dp);
    dentry_unlock(old_dp);
}

void dentry_free(struct dentry *dp)
{
    // when called from drain LRU
    DEBUG_EXTRA_PANIC(kref_read(&dp->ref) == 0,
                      "dentry_free: reference count too large");

    // drop inode ref
    if (dp->ip != NULL)
    {
        inode_put(dp->ip);
        dp->ip = NULL;
    }

    // free name
    if (dp->name != NULL)
    {
        kfree((char *)dp->name);
    }

    // free dentry itself
    kfree(dp);
}

struct dentry *dentry_get(struct dentry *dp)
{
    DEBUG_EXTRA_PANIC(dp != NULL, "dentry_get: dp is NULL");

    int previous = kref_get_and_return_previous(&dp->ref);
    if (previous == 0)
    {
        // re-use dentry from LRU list
        spin_lock(&g_dentry_cache.list_lock);
        list_del(&dp->lru_list);
        g_dentry_cache.lru_size--;
        spin_unlock(&g_dentry_cache.list_lock);
    }
    return dp;
}

void dentry_put(struct dentry *dp)
{
    DEBUG_EXTRA_PANIC(dp != NULL, "dentry_put: dp is NULL");

    if (kref_put(&dp->ref) == false)
    {
        // Still references left
        return;
    }

    // No references left, move to recently used list if still linked in tree
    if (dentry_is_unlinked(dp))
    {
        dentry_cache_remove_from_unlinked(dp);
        dentry_free(dp);
        return;
    }
    else if (dp->parent != NULL)
    {
        // still linked in the dentry tree
        dentry_cache_move_to_lru(dp);
        return;
    }
    else
    {
        // not linked in tree anymore -> free
        dentry_free(dp);
    }
}
