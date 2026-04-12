/* SPDX-License-Identifier: MIT */

#include <fs/dentry_cache.h>
#include <fs/dentry_cache_sysfs.h>
#include <kernel/container_of.h>
#include <kernel/major.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <lib/minmax.h>
#include <mm/kernel_memory.h>

struct dentry_cache g_dentry_cache = {0};

//
// dentry cache
//

struct dentry *dentry_cache_init(struct inode *root_ip)
{
    g_dentry_cache.root = NULL;
    rwspin_lock_init(&g_dentry_cache.tree_lock, "dcache_tree");
    list_init(&g_dentry_cache.lru_list);
    list_init(&g_dentry_cache.unlinked_list);
    spin_lock_init(&g_dentry_cache.list_lock, "dentry lists");
    g_dentry_cache.lru_size = 0;
    g_dentry_cache.max_lru_size = 16;

    struct dentry *root_dentry = dentry_alloc();
    if (root_dentry == NULL)
    {
        panic("dentry_cache_init: failed to allocate root dentry");
    }

    root_dentry->ip = inode_get(root_ip);
    root_dentry->name = "/";

    g_dentry_cache.root = root_dentry;
    g_dentry_cache.unlinked_root = dentry_alloc_init_orphan("(unlinked)", NULL);
    if (g_dentry_cache.unlinked_root == NULL)
    {
        panic("dentry_cache_init: failed to allocate unlinked root dentry");
    }

    kobject_init(&g_dentry_cache.kobj, &dentry_cache_kobj_ktype);
    kobject_add(&g_dentry_cache.kobj, &g_kernel_memory.kobj, "dcache");

    return root_dentry;
}

struct dentry *dentry_cache_get_root()
{
    DEBUG_EXTRA_PANIC(g_dentry_cache.root != NULL,
                      "dentry_cache_init() never called!");
    return dentry_get(g_dentry_cache.root);
}

void dentry_cache_drain_lru(struct dentry_cache *cache, size_t target)
{
    enum
    {
        DRAIN_BATCH_SIZE = 32
    };
    struct dentry *to_free[DRAIN_BATCH_SIZE];
    struct dentry *parents_to_put[DRAIN_BATCH_SIZE];

    while (cache->lru_size > target)
    {
        size_t drained = 0;

        // detach a bounded batch under locks
        dcache_write_lock();
        spin_lock(&cache->list_lock);
        while (drained < DRAIN_BATCH_SIZE && cache->lru_size > target)
        {
            struct list_head *lru_pos = cache->lru_list.prev;
            struct dentry *lru_dp = dentry_from_lru_list(lru_pos);

            if (kref_read(&lru_dp->ref) != 0)
            {
                // can't evict least-recently-used candidate right now
                break;
            }

            list_del(&lru_dp->lru_list);
            cache->lru_size--;

            struct dentry *parent = lru_dp->parent;
            if (parent != NULL)
            {
                list_del(&lru_dp->sibling_list);
                lru_dp->parent = NULL;
            }

            to_free[drained] = lru_dp;
            parents_to_put[drained] = parent;
            drained++;
        }
        spin_unlock(&cache->list_lock);
        dcache_write_unlock();

        if (drained == 0)
        {
            // no progress possible right now
            break;
        }

        // drop parent refs and free dentries outside locks
        for (size_t i = 0; i < drained; ++i)
        {
            if (parents_to_put[i] != NULL)
            {
                dentry_put(parents_to_put[i]);
            }
            dentry_free(to_free[i]);
        }
    }
}

void dentry_cache_move_to_lru(struct dentry *dp)
{
    spin_lock(&g_dentry_cache.list_lock);
    if (list_empty(&dp->lru_list))
    {
        list_add(&dp->lru_list, &g_dentry_cache.lru_list);
        g_dentry_cache.lru_size++;
    }
    spin_unlock(&g_dentry_cache.list_lock);

    dentry_cache_drain_lru(&g_dentry_cache, g_dentry_cache.max_lru_size);
}

struct dentry *dentry_cache_lookup_tree_locked(struct dentry *parent,
                                               const char *name)
{
    struct dentry *result = NULL;

    struct list_head *pos;
    list_for_each(pos, &parent->child_list)
    {
        struct dentry *child = dentry_from_child_list(pos);
        if (strcmp(child->name, name) == 0)
        {
            result = dentry_get(child);
            break;
        }
    }

    return result;
}

struct dentry *dentry_cache_lookup(struct dentry *parent, const char *name)
{
    struct dentry *result = NULL;

    dcache_read_lock();
    result = dentry_cache_lookup_tree_locked(parent, name);
    dcache_read_unlock();

    return result;
}

struct dentry *dentry_cache_add(struct dentry *parent, const char *name,
                                struct inode *ip)
{
    // alloc new dentry
    struct dentry *new_dentry = dentry_alloc_init_orphan(name, ip);
    if (new_dentry == NULL)
    {
        panic("dentry_cache_add: out of memory");
        return NULL;
    }

    dcache_write_lock();
    struct dentry *tmp = dentry_cache_lookup_tree_locked(parent, name);
    if (tmp != NULL)
    {
        // dentry already exists.
        // This possibility is unavoidable as new dentries might require disk IO
        // to read the inode data (and thus might require yielding). So the
        // dentry cache update logic "entry-not-found -> read inode -> create
        // dentry" can not be atomic (can't hold dentry lock while sleeping).

        // don't use dentry_put() as it should not end up in the LRU list but
        // get freed instantly.
        kref_put(&new_dentry->ref);
        dentry_free(new_dentry);
        dcache_write_unlock();
        return tmp;
    }
    else
    {
        dentry_register_with_parent(parent, new_dentry);
    }
    dcache_write_unlock();

    return new_dentry;
}

void dentry_cache_add_to_unlinked(struct dentry *dp)
{
    dcache_write_lock();
    dentry_register_with_parent(g_dentry_cache.unlinked_root, dp);
    dcache_write_unlock();
}

void dentry_cache_remove_from_unlinked(struct dentry *dp)
{
    struct dentry *parent_to_put = NULL;

    dcache_write_lock();
    if (dp->parent != NULL)
    {
        list_del(&dp->sibling_list);
        parent_to_put = dp->parent;
        dp->parent = NULL;
    }
    dcache_write_unlock();

    if (parent_to_put != NULL)
    {
        dentry_put(parent_to_put);
    }
}

syserr_t dentry_cache_set_max_lru(struct dentry_cache *dcache,
                                  size_t max_lru_size)
{
    spin_lock(&dcache->list_lock);
    dcache->max_lru_size = max_lru_size;
    spin_unlock(&dcache->list_lock);

    dentry_cache_drain_lru(dcache, max_lru_size);

    return 0;
}

syserr_t dentry_cache_clear_lru(struct dentry_cache *dcache)
{
    dentry_cache_drain_lru(dcache, 0);
    return 0;
}

//
// Debug print
//

static inline void print_spaces(size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        printk(" ");
    }
}

void debug_print_dentry_cache_node(struct dentry *dp, size_t level)
{
    if (level == 37)
    {
        printk("...\n");
    }
    print_spaces(level * 4);
    size_t name_len = strlen(dp->name);
    printk("%s", dp->name);
    ssize_t printed = level * 4 + name_len;
    print_spaces(max(32 - printed, 1));
    printk("(ref %d) ", kref_read(&dp->ref));
    printk("(ref %d 0x%zx ", kref_read(&dp->ref), (size_t)dp);
    if (atomic_load_explicit(&dp->lock.locked, memory_order_relaxed))
    {
        printk("(locked) ");
    }

    if (dp->ip == NULL)
    {
        printk("(invalid)\n");
    }
    else
    {
        debug_print_inode(dp->ip);
        printk("\n");
    }

    struct list_head *pos, *n;
    list_for_each_safe(pos, n, &dp->child_list)
    {
        struct dentry *child = dentry_from_child_list(pos);
        debug_print_dentry_cache_node(child, level + 1);
    }
}

void debug_print_dentry_list(struct list_head *list)
{
    struct list_head *pos;
    list_for_each(pos, list)
    {
        struct dentry *dp = dentry_from_lru_list(pos);
        debug_print_dentry_cache_node(dp, 1);
    }
}

void debug_print_dentry_cache()
{
    if (g_dentry_cache.root == NULL)
    {
        printk("Dentry cache is empty\n");
        return;
    }

    printk("Dentry cache:\n");
    debug_print_dentry_cache_node(g_dentry_cache.root, 0);
    printk("LRU list usage: %zu / %zu\n", g_dentry_cache.lru_size,
           g_dentry_cache.max_lru_size);
    printk("unlinked list:\n");
    debug_print_dentry_cache_node(g_dentry_cache.unlinked_root, 0);

    printk("\n");
}

void debug_print_path(struct dentry *dentry)
{
    if (dentry == NULL)
    {
        printk("(null)");
        return;
    }
    if (dentry->parent == NULL)
    {
        printk("/");
        return;
    }
    else
    {
        debug_print_path(dentry->parent);
        if (dentry->parent->parent != NULL)
        {
            printk("/");
        }
    }

    printk("%s", dentry->name);
}
