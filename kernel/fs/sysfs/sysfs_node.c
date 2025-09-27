/* SPDX-License-Identifier: MIT */

#include <fs/sysfs/sysfs_node.h>
#include <kernel/string.h>
#include <mm/kalloc.h>

void sysfs_nodes_init(struct super_block *sb)
{
    struct sysfs_sb_private *priv = (struct sysfs_sb_private *)sb->s_fs_info;
    rwspin_lock_init(&priv->lock, "sysfs_node_tree_lock");
    priv->root = NULL;
}

void sysfs_nodes_deinit(struct super_block *sb)
{
    struct sysfs_sb_private *priv = (struct sysfs_sb_private *)sb->s_fs_info;

    rwspin_write_lock(&priv->lock);
    sysfs_node_free(priv->root, priv);
    rwspin_write_unlock(&priv->lock);
}

struct sysfs_node *sysfs_node_alloc_init(struct kobject *kobj,
                                         size_t sysfs_entry_index,
                                         struct sysfs_sb_private *priv)
{
    // index 0 is the dir itself, 1..n are the entries
    if (sysfs_entry_index >= (kobj->ktype->n_attributes + 1))
    {
        return NULL;  // invalid index
    }
    // stays NULL for index 0 / dir itself
    struct sysfs_attribute *entry = NULL;
    if (sysfs_entry_index > 0)
    {
        entry = &kobj->ktype->attribute[sysfs_entry_index - 1];
    }

    struct sysfs_node *node =
        (struct sysfs_node *)kmalloc(sizeof(struct sysfs_node));
    if (node == NULL) return NULL;

    memset(node, 0, sizeof(struct sysfs_node));

    ino_t inode_number = atomic_fetch_add(&priv->next_free_inum, 1);
    node->inode_number = inode_number;
    node->name = (entry != NULL) ? entry->name : kobj->name;
    node->kobj = kobj;
    node->sysfs_node_index = sysfs_entry_index;
    node->attribute = entry;

    list_init(&node->child_list);
    list_init(&node->sibling_list);

    // protect sysfs specifics in kobj and the addition to the parents child
    // list
    rwspin_write_lock(&priv->lock);
    kobj->sysfs_nodes[sysfs_entry_index] = node;
    node->sysfs_ip = NULL;

    // find parent:
    if (sysfs_entry_index == 0)
    {
        // the dir itself
        if (kobj->parent == NULL)
        {
            // no parent -> root
            priv->root = node;
            node->parent = NULL;
        }
        else
        {
            struct sysfs_node *parent = kobj->parent->sysfs_nodes[0];
            node->parent = parent;
            list_add(&node->sibling_list, &parent->child_list);
        }
    }
    else
    {
        // an attribute -> file in the dir
        node->parent = kobj->sysfs_nodes[0];
        list_add(&node->sibling_list, &node->parent->child_list);
    }
    rwspin_write_unlock(&priv->lock);

    return node;
}

void sysfs_node_free(struct sysfs_node *node, struct sysfs_sb_private *priv)
{
    if (node == NULL) return;
    DEBUG_ASSERT_CPU_HOLDS_RWLOCK(&priv->lock);

    // free children first
    struct list_head *pos;
    for (pos = node->child_list.next; !(pos == (node->child_list.next));)
    {
        struct sysfs_node *child_node = sysfs_node_from_child_list(pos);
        pos = pos->next;  // save next pointer as child_node will be freed
        sysfs_node_free(child_node, priv);
    }

    node->kobj->sysfs_nodes[node->sysfs_node_index] = NULL;

    list_del(&node->sibling_list);
    kfree(node);
}

struct sysfs_node *sysfs_find_node_locked(struct sysfs_node *start, ino_t inum)
{
    if (start->inode_number == inum)
    {
        return start;
    }

    struct list_head *pos;
    list_for_each(pos, &start->child_list)
    {
        struct sysfs_node *child = sysfs_node_from_child_list(pos);
        struct sysfs_node *result = sysfs_find_node_locked(child, inum);
        if (result != NULL)
        {
            return result;
        }
    }
    return NULL;
}

struct sysfs_node *sysfs_find_node(struct sysfs_sb_private *priv, ino_t inum)
{
    rwspin_read_lock(&priv->lock);
    struct sysfs_node *result = sysfs_find_node_locked(priv->root, inum);
    rwspin_read_unlock(&priv->lock);
    return result;
}

void debug_print_sysfs_node_depth(struct sysfs_node *node, size_t depth)
{
    static size_t node_count = 0;

    for (size_t i = 0; i <= depth; i++)
    {
        printk("  ");
    }
    printk("%s (inode: %ld)\n", node->name, node->inode_number);
    node_count++;

    struct list_head *pos;
    list_for_each(pos, &node->child_list)
    {
        struct sysfs_node *child = sysfs_node_from_child_list(pos);

        debug_print_sysfs_node_depth(child, depth + 1);
    }

    if (depth == 0)
    {
        printk("Total sysfs nodes: %zu\n", node_count);
    }
}

void debug_print_sysfs_node(struct sysfs_node *node)
{
    // rwspin_read_lock(&node->kobj->children_lock);
    debug_print_sysfs_node_depth(node, 0);
    // rwspin_read_unlock(&node->kobj->children_lock);
}
