/* SPDX-License-Identifier: MIT */

#include <fs/sysfs/sysfs.h>
#include <kernel/kalloc.h>
#include <kernel/kobject.h>
#include <kernel/rwspinlock.h>
#include <kernel/stdarg.h>
#include <kernel/string.h>

extern char __start_data[];  // in kernel.ld
extern char __end_data[];    // in kernel.ld

static inline bool is_kernel_data(size_t addr)
{
    return ((addr >= (size_t)__start_data) && (addr < (size_t)__end_data));
}

struct kobject g_kobjects_root = {0};  // the root of all kobjects
struct kobject g_kobjects_proc = {0};  // all processes
struct kobject g_kobjects_dev = {0};   // all devices
struct kobject g_kobjects_fs = {0};    // all filesystems

static void dynamic_kobj_release(struct kobject *kobj) { kfree(kobj); }

static const struct kobj_type dynamic_kobj_ktype = {.release =
                                                        dynamic_kobj_release};

const struct kobj_type default_kobj_ktype = {
    .release = NULL, .sysfs_ops = NULL, .attribute = NULL, .n_attributes = 0};

static inline void init_kobjects_in_root(struct kobject *kobj, const char *name)
{
    kobject_init(kobj, &default_kobj_ktype);
    kobject_add(kobj, &g_kobjects_root, name);
    kobject_put(kobj);
}

void init_kobject_root()
{
    kobject_init(&g_kobjects_root, &default_kobj_ktype);
    g_kobjects_root.name = "sys";
    g_kobjects_root.parent = NULL;

    init_kobjects_in_root(&g_kobjects_dev, "dev");
    init_kobjects_in_root(&g_kobjects_fs, "fs");
    init_kobjects_in_root(&g_kobjects_proc, "proc");
}

struct kobject *kobject_create_init()
{
    struct kobject *kobj = kmalloc(sizeof(*kobj));
    if (kobj == NULL) return NULL;

    memset(kobj, 0, sizeof(*kobj));
    kobject_init(kobj, &dynamic_kobj_ktype);
    return kobj;
}

void kobject_init(struct kobject *kobj, const struct kobj_type *ktype)
{
    list_init(&kobj->children);
    list_init(&kobj->siblings);
    if (ktype == NULL)
    {
        kobj->ktype = &default_kobj_ktype;
    }
    else
    {
        kobj->ktype = ktype;
    }
    kref_init(&kobj->ref_count);
    rwspin_lock_init(&kobj->children_lock, "kobj_children");
}

bool kobject_add_varg(struct kobject *kobj, struct kobject *parent,
                      const char *fmt, va_list vargs)
{
    if (strchr(fmt, '%') == NULL && is_kernel_data((size_t)fmt))
    {
        // fmt is a constant string, just use it
        kobj->name = fmt;
    }
    else
    {
        const size_t MAX_NAME_LEN = 64;
        char *name = kmalloc(MAX_NAME_LEN);
        if (name == NULL) return false;
        memset(name, 0, MAX_NAME_LEN);
        vsnprintf(name, MAX_NAME_LEN, fmt, vargs);
        kobj->name = name;
    }

    kobj->parent = parent;
    rwspin_write_lock(&parent->children_lock);
    list_add_tail(&kobj->siblings, &parent->children);
    rwspin_write_unlock(&parent->children_lock);

    kobject_get(kobj);    // the parent holds a reference to each child
    kobject_get(parent);  // child holds a reference to its parent

    // init stuff for sysfs
    spin_lock_init(&kobj->sysfs_lock, "kobj_sysfs");

    sysfs_register_kobject(kobj);

    return true;
}

bool kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt,
                 ...)
{
    if (fmt == NULL || parent == NULL || kobj == NULL) return false;

    bool ret;
    va_list args;
    va_start(args, fmt);
    ret = kobject_add_varg(kobj, parent, fmt, args);
    va_end(args);

    return ret;
}

void kobject_del(struct kobject *kobj)
{
    if (kobj == NULL || kobj->parent == NULL) return;

    sysfs_unregister_kobject(kobj);

    rwspin_write_lock(&kobj->parent->children_lock);
    list_del(&kobj->siblings);
    rwspin_write_unlock(&kobj->parent->children_lock);

    kobject_put(kobj);  // drop reference held by parent
}

void kobject_put(struct kobject *kobj)
{
    DEBUG_EXTRA_PANIC(kobj != NULL, "kobject_put() on NULL");

    if (kref_put(&kobj->ref_count) == true)
    {
        // name could be a constant string, check before freeing
        if ((kobj->name != NULL) &&
            (is_kernel_data((size_t)kobj->name) == false))
        {
            kfree((void *)kobj->name);
        }

        // can be NULL if the object wasn't completely initialized
        if (kobj->parent != NULL)
        {
            kobject_put(kobj->parent);  // drop reference held by child
        }

        if (kobj->ktype && kobj->ktype->release)
        {
            kobj->ktype->release(kobj);
        }
    }
}

size_t debug_print_kobject_node(struct kobject *kobj, size_t depth)
{
    static size_t node_count = 0;

    for (size_t i = 0; i <= depth; i++)
    {
        printk("  ");
    }
    printk("%s (refcount: %d)\n", kobj->name ? kobj->name : "(null)",
           kref_read(&kobj->ref_count));
    node_count++;

    struct list_head *pos;
    rwspin_read_lock(&kobj->children_lock);
    list_for_each(pos, &kobj->children)
    {
        struct kobject *child = kobject_from_child_list(pos);

        debug_print_kobject_node(child, depth + 1);
    }
    rwspin_read_unlock(&kobj->children_lock);

    return node_count;
}

void debug_print_kobject_tree()
{
    printk("\nKobject tree:\n");

    size_t node_count = debug_print_kobject_node(&g_kobjects_root, 0);

    printk("Total kobjects: %zd\n", node_count);
}
