/* SPDX-License-Identifier: MIT */

#include <fs/dentry.h>
#include <fs/dentry_cache.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/kalloc.h>

//
// dentry management
//

// Internal dentry-cache eviction helper.
void dentry_cache_drain_lru(struct dentry_cache *cache, size_t target);

struct dentry_tree_guard g_dentry_tree_guard = {0};
struct dentry_lru_guard g_dentry_lru_guard = {0};

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
    dentry_set_inode(new_dentry, NULL);
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
        dentry_set_inode(new_dentry, inode_get(ip));
    }
    else
    {
        dentry_set_inode(new_dentry, NULL);
    }

    return new_dentry;
}

void dentry_register_with_parent(struct dentry *parent, struct dentry *child)
{
#ifdef CONFIG_DEBUG_SPINLOCK
    bool has_parent_lock = spin_lock_is_held_by_this_cpu(&parent->lock);
    bool has_tree_write_lock =
        rwspin_write_lock_is_held_by_this_cpu(&g_dentry_cache.tree_lock);
    DEBUG_EXTRA_PANIC(has_parent_lock || has_tree_write_lock,
                      "dentry_register_with_parent: missing required lock");
#endif

    child->parent = dentry_get(parent);
    list_add(&child->sibling_list, &parent->child_list);
}

struct dentry *dentry_unregister_from_parent(struct dentry *child)
{
    DEBUG_EXTRA_PANIC(child != NULL,
                      "dentry_unregister_from_parent: child is NULL");
    if (child->parent == NULL)
    {
        return NULL;
    }

#ifdef CONFIG_DEBUG_SPINLOCK
    bool has_parent_lock = spin_lock_is_held_by_this_cpu(&child->parent->lock);
    bool has_tree_write_lock =
        rwspin_write_lock_is_held_by_this_cpu(&g_dentry_cache.tree_lock);
    DEBUG_EXTRA_PANIC(has_parent_lock || has_tree_write_lock,
                      "dentry_unregister_from_parent: missing required lock");
#endif

    struct dentry *parent = child->parent;
    list_del(&child->sibling_list);
    child->parent = NULL;
    return parent;
}

struct dentry *dentry_alloc_init(struct dentry *parent, const char *name,
                                 struct inode *ip)
{
    struct dentry *new_dentry = dentry_alloc_init_orphan(name, ip);
    if (new_dentry == NULL)
    {
        return NULL;
    }

    dcache_write_lock();
    dentry_register_with_parent(parent, new_dentry);
    dcache_write_unlock();

    return new_dentry;
}

void dentry_switch_children(struct dentry *old_dp, struct dentry *new_dp)
{
    dcache_write_lock();
    DEBUG_EXTRA_ASSERT(old_dp->parent != NULL,
                       "dentry_switch_children: old_dp has no parent");

    list_del(&old_dp->sibling_list);
    list_add(&new_dp->sibling_list, &old_dp->parent->child_list);
    // skip get/put ownership transfer, "move" ownership
    new_dp->parent = old_dp->parent;
    old_dp->parent = NULL;

    dcache_write_unlock();
}

void dentry_free(struct dentry *dp)
{
    // when called from drain LRU
    DEBUG_EXTRA_PANIC(kref_read(&dp->ref) == 0,
                      "dentry_free: reference count too large");

    // drop inode ref
    struct inode *ip = dentry_inode(dp);
    if (ip != NULL)
    {
        DEBUG_EXTRA_ASSERT(kref_read(&ip->ref) > 0,
                           "dentry inode reference was already released");
        inode_put(ip);
        dentry_set_inode(dp, NULL);
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

    dcache_list_lock(&g_dentry_cache);
    int previous = kref_get_and_return_previous(&dp->ref);
    if (previous == 0)
    {
        // re-use dentry from LRU list
        if (!list_empty(&dp->lru_list))
        {
            list_del(&dp->lru_list);
            g_dentry_cache.lru_size--;
        }
    }
    dcache_list_unlock(&g_dentry_cache);
    return dp;
}

void dentry_put(struct dentry *dp)
{
    DEBUG_EXTRA_PANIC(dp != NULL, "dentry_put: dp is NULL");

    dcache_read_lock();
    dcache_list_lock(&g_dentry_cache);

    if (kref_put(&dp->ref) == false)
    {
        // Still references left
        dcache_list_unlock(&g_dentry_cache);
        dcache_read_unlock();
        return;
    }

    bool is_unlinked = dentry_is_unlinked(dp);
    bool is_linked = dp->parent != NULL;

    if (is_linked && !is_unlinked)
    {
        // Still linked in the discoverable dentry tree. Cache it with ref 0.
        DEBUG_EXTRA_ASSERT(list_empty(&dp->lru_list),
                           "zero-reference dentry already on LRU");
        list_add(&dp->lru_list, &g_dentry_cache.lru_list);
        g_dentry_cache.lru_size++;
        size_t max_lru_size = g_dentry_cache.max_lru_size;
        dcache_list_unlock(&g_dentry_cache);
        dcache_read_unlock();

        dentry_cache_drain_lru(&g_dentry_cache, max_lru_size);
        return;
    }

    dcache_list_unlock(&g_dentry_cache);
    dcache_read_unlock();

    if (is_unlinked)
    {
        dentry_cache_remove_from_unlinked(dp);
        dentry_free(dp);
        return;
    }
    else
    {
        // not linked in tree anymore -> free
        dentry_free(dp);
    }
}

size_t dentry_get_cwd_length(struct dentry *dentry)
{
    size_t len = 0;
    if (dentry == NULL)
    {
        return 0;
    }
    if (dentry->parent == NULL)
    {
        return 2;  // "/" + trailing zero
    }
    else
    {
        len = dentry_get_cwd_length(dentry->parent);
        if (dentry->parent->parent != NULL)
        {
            len++;
        }
    }

    return len + strlen(dentry->name);
}

size_t dentry_get_cwd(struct dentry *dentry, char *buf, size_t buf_len)
{
    size_t printed_total = 0;

    if (dentry == NULL)
    {
        return printed_total;
    }

    if (dentry->parent == NULL)
    {
        return snprintf(buf, buf_len, "/");
    }
    else
    {
        size_t printed = dentry_get_cwd(dentry->parent, buf, buf_len);
        buf_len += printed;
        buf += printed;
        printed_total += printed;

        if (dentry->parent->parent != NULL)
        {
            size_t printed = snprintf(buf, buf_len, "/");
            buf_len += printed;
            buf += printed;
            printed_total += printed;
        }
    }

    printed_total += snprintf(buf, buf_len, "%s", dentry->name);
    return printed_total;
}
