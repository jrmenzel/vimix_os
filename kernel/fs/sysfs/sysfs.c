/* SPDX-License-Identifier: MIT */

#include <drivers/device.h>
#include <fs/sysfs/sysfs.h>
#include <fs/sysfs/sysfs_internal.h>
#include <fs/sysfs/sysfs_node.h>
#include <fs/sysfs/sysfs_sb_priv.h>
#include <kernel/errno.h>
#include <kernel/kalloc.h>
#include <kernel/param.h>
#include <kernel/proc.h>
#include <kernel/string.h>
#include <lib/minmax.h>

struct file_system_type sysfs_file_system_type;
const char *SYS_FS_NAME = "sysfs";

// super block operations
struct super_operations sysfs_s_op = {
    iget_root : sysfs_sops_iget_root,
    alloc_inode : sops_alloc_inode_default_ro,
    write_inode : sops_write_inode_default_ro
};

// inode operations
struct inode_operations sysfs_i_op = {
    iops_create : iops_create_default_ro,
    iops_open : sysfs_iops_open,
    iops_read_in : sysfs_iops_read_in,
    iops_dup : iops_dup_default,
    iops_put : sysfs_iops_put,
    iops_dir_lookup : sysfs_iops_dir_lookup,
    iops_dir_link : iops_dir_link_default_ro,
    iops_get_dirent : sysfs_iops_get_dirent,
    iops_read : sysfs_iops_read,
    iops_link : iops_link_default_ro,
    iops_unlink : iops_unlink_default_ro
};

struct file_operations sysfs_f_op = {fops_write : sysfs_fops_write};

// only one sysfs is allowed
struct super_block *sysfs_super_block = NULL;

struct sysfs_node *sysfs_register_kobject_parent(
    struct kobject *kobj, struct sysfs_node *parent_sys_node);

static inline ino_t sysfs_get_parent_inode_number(struct sysfs_inode *sys_ip)
{
    if (sys_ip->node->parent == NULL)
    {
        return INVALID_INODE;
    }
    return sys_ip->node->parent->inode_number;
}

void sysfs_register_kobject_and_children(struct kobject *kobj,
                                         struct sysfs_node *parent_sys_node)
{
    struct sysfs_node *node_parent =
        sysfs_register_kobject_parent(kobj, parent_sys_node);

    // recurse into children
    rwspin_write_lock(&kobj->children_lock);
    struct list_head *pos;
    list_for_each(pos, &kobj->children)
    {
        struct kobject *child_kobj = kobject_from_child_list(pos);
        sysfs_register_kobject_and_children(child_kobj, node_parent);
    }
    rwspin_write_unlock(&kobj->children_lock);
}

void sysfs_init()
{
    sysfs_file_system_type.name = SYS_FS_NAME;

    sysfs_file_system_type.next = NULL;
    sysfs_file_system_type.init_fs_super_block = sysfs_init_fs_super_block;
    sysfs_file_system_type.kill_sb = sysfs_kill_sb;

    register_file_system(&sysfs_file_system_type);
}

ssize_t sysfs_init_fs_super_block(struct super_block *sb_in, const void *data)
{
    struct sysfs_sb_private *priv =
        (struct sysfs_sb_private *)kmalloc(sizeof(struct sysfs_sb_private));
    if (priv == NULL)
    {
        return -ENOMEM;
    }
    memset(priv, 0, sizeof(struct sysfs_sb_private));
    atomic_init(&priv->next_free_inum,
                1);  // start with inode 1, 0 is reserved
    sb_in->s_fs_info = (void *)priv;

    sb_in->s_type = &sysfs_file_system_type;
    sb_in->s_op = &sysfs_s_op;
    sb_in->i_op = &sysfs_i_op;
    sb_in->f_op = &sysfs_f_op;
    sb_in->dev = MKDEV(SYSFS_MAJOR, 0);

    sysfs_super_block = sb_in;
    sysfs_nodes_init(sb_in);
    sysfs_register_kobject_and_children(&g_kobjects_root, NULL);

    return 0;
}

void sysfs_kill_sb(struct super_block *sb_in)
{
    sysfs_nodes_deinit(sb_in);
    kfree(sb_in->s_fs_info);
    sb_in->s_fs_info = NULL;
    sysfs_super_block = NULL;
}

struct sysfs_inode *sysfs_create_inode_from_node(struct sysfs_node *node)
{
    struct super_block *sb = sysfs_super_block;

    struct sysfs_inode *sys_ip = kmalloc(sizeof(struct sysfs_inode));
    if (sys_ip == NULL)
    {
        return NULL;
    }
    memset(sys_ip, 0, sizeof(struct sysfs_inode));

    // init base inode
    inode_init(&sys_ip->ino, sb, node->inode_number);
    sys_ip->ino.valid = true;  // inode has been "read from disk"
    sys_ip->ino.i_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    if (node->attribute != NULL)
    {
        sys_ip->ino.i_mode |= S_IFREG;
        sys_ip->ino.size = 1024;
    }
    else
    {
        sys_ip->ino.i_mode |= S_IFDIR;
        sys_ip->ino.size = 0;
    }
    sys_ip->ino.nlink = 1;

    // init sysfs specifics
    sys_ip->node = node;
    node->sysfs_ip = sys_ip;

    return sys_ip;
}

void sysfs_register_kobject(struct kobject *kobj)
{
    struct kobject *parent = kobj->parent;
    if ((parent == NULL) || (parent->sysfs_nodes == NULL))
    {
        // root kobject or parent not registered
        sysfs_register_kobject_parent(kobj, NULL);
        return;
    }

    struct sysfs_node *parent_sys_node = parent->sysfs_nodes[0];
    sysfs_register_kobject_parent(kobj, parent_sys_node);
}

struct sysfs_node *sysfs_register_kobject_parent(
    struct kobject *kobj, struct sysfs_node *parent_sys_ip)
{
    if (sysfs_super_block == NULL)
    {
        // ignore if sysfs not initialized yet
        // sysfs_init fill register all kobjects,
        // this function is only relevant for later created objects
        return NULL;
    }

    DEBUG_EXTRA_PANIC(kobj->ktype != NULL,
                      "sysfs_register_kobject_parent: kobj->ktype is NULL");

    kobj->sysfs_nodes =
        kmalloc(sizeof(struct sysfs_node *) * (kobj->ktype->n_attributes + 1));
    if (kobj->sysfs_nodes == NULL) return NULL;
    memset(kobj->sysfs_nodes, 0,
           sizeof(struct sysfs_node *) * (kobj->ktype->n_attributes + 1));

    struct sysfs_sb_private *priv =
        (struct sysfs_sb_private *)sysfs_super_block->s_fs_info;
    struct sysfs_node *dir_node = sysfs_node_alloc_init(kobj, 0, priv);
    if (dir_node == NULL)
    {
        printk(
            "sysfs_register_kobject_parent: failed to create sysfs node for "
            "kobj "
            "%s\n",
            kobj->name);
        return NULL;
    }

    for (size_t i = 0; i < kobj->ktype->n_attributes; i++)
    {
        struct sysfs_node *node = sysfs_node_alloc_init(kobj, i + 1, priv);
        if (node == NULL)
        {
            printk(
                "sysfs_register_kobject_parent: failed to create inode for "
                "kobj "
                "%s\n",
                kobj->name);
            break;
        }
    }

    return dir_node;
}

void sysfs_unregister_kobject(struct kobject *kobj)
{
    if (sysfs_super_block == NULL)
    {
        // ignore if sysfs not initialized yet
        return;
    }

    struct sysfs_sb_private *priv =
        (struct sysfs_sb_private *)sysfs_super_block->s_fs_info;
    struct sysfs_node *node_dir = kobj->sysfs_nodes[0];
    rwspin_write_lock(&priv->lock);
    sysfs_node_free(node_dir, priv);  // frees all children as well
    rwspin_write_unlock(&priv->lock);

    kfree(kobj->sysfs_nodes);
}

/// @brief Find an inode by its inode number, inode must be in memory. Inode
/// list must be locked externally.
/// @param sb sysfs super block
/// @param inum inode number to search for
/// @return The found inode or NULL if not found
struct inode *sysfs_find_inode_locked(struct super_block *sb, ino_t inum)
{
    struct list_head *pos;
    list_for_each(pos, &sb->fs_inode_list)
    {
        struct inode *ip = inode_from_list(pos);
        if (ip->inum == inum)
        {
            inode_get(ip);
            return ip;
        }
    }
    return NULL;
}

struct inode *sysfs_get_inode_from_node(struct super_block *sb,
                                        struct sysfs_node *node)
{
    if (node == NULL)
    {
        return NULL;
    }
    if (node->sysfs_ip != NULL)
    {
        inode_get(&node->sysfs_ip->ino);
        return &node->sysfs_ip->ino;
    }

    rwspin_write_lock(&sb->fs_inode_list_lock);
    struct sysfs_inode *ip = sysfs_create_inode_from_node(node);
    rwspin_write_unlock(&sb->fs_inode_list_lock);

    return &ip->ino;
}

/// @brief Find an inode by its inode number. If it doesn't exist yet, create
/// it.
/// @param sb sysfs super block
/// @param inum inode number to search for
/// @return The found inode or NULL if not found
struct inode *sysfs_find_inode(struct super_block *sb, ino_t inum)
{
    rwspin_read_lock(&sb->fs_inode_list_lock);
    struct inode *ip = sysfs_find_inode_locked(sb, inum);
    rwspin_read_unlock(&sb->fs_inode_list_lock);

    if (ip != NULL)
    {
        // found
        return ip;
    }

    struct sysfs_node *sysfs_node =
        sysfs_find_node((struct sysfs_sb_private *)sb->s_fs_info, inum);
    if (sysfs_node == NULL)
    {
        // not found
        return NULL;
    }

    // not found -> create it
    rwspin_write_lock(&sb->fs_inode_list_lock);
    // double check if it wasn't created in the meantime
    ip = sysfs_find_inode_locked(sb, inum);
    if (ip != NULL)
    {
        rwspin_write_unlock(&sb->fs_inode_list_lock);
        return ip;
    }

    struct sysfs_inode *sysfs_ip = sysfs_create_inode_from_node(sysfs_node);

    rwspin_write_unlock(&sb->fs_inode_list_lock);

    return &sysfs_ip->ino;
}

struct inode *sysfs_sops_iget_root(struct super_block *sb)
{
    struct inode *ip = sysfs_find_inode(sb, 1);
    return ip;
}

struct inode *sysfs_iops_open(struct inode *iparent, char name[NAME_MAX],
                              int32_t flags)
{
    inode_lock(iparent);
    struct inode *ip = sysfs_iops_dir_lookup(iparent, name, NULL);
    inode_unlock(iparent);
    if (ip == NULL)
    {
        // file not found
        return NULL;
    }
    inode_lock(ip);

#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
    strncpy(ip->path, name, PATH_MAX);
#endif
    return ip;  // return locked
}

void sysfs_iops_read_in(struct inode *ip) { printk("sysfs_iops_read_in\n"); }

void sysfs_iops_put(struct inode *ip)
{
    DEBUG_EXTRA_ASSERT(kref_read(&ip->ref) > 0,
                       "Can't put an inode that is not held by anyone");

    if (kref_put(&ip->ref) == true)
    {
        // last reference dropped
        rwspin_write_lock(&ip->i_sb->fs_inode_list_lock);
        if (kref_read(&ip->ref) > 0)
        {
            // someone else got a reference in the meantime
            rwspin_write_unlock(&ip->i_sb->fs_inode_list_lock);
            return;
        }
        inode_del(ip);
        struct sysfs_inode *sysfs_ip = sysfs_inode_from_inode(ip);
        sysfs_ip->node->sysfs_ip = NULL;  // tell the node that inode is gone
        rwspin_write_unlock(&ip->i_sb->fs_inode_list_lock);
        kfree(sysfs_ip);
    }
}

struct inode *sysfs_iops_dir_lookup(struct inode *dir, const char *name,
                                    uint32_t *poff)
{
    if (!S_ISDIR(dir->i_mode)) return NULL;

    struct sysfs_inode *sysfs_dir = sysfs_inode_from_inode(dir);

    if (strcmp(name, ".") == 0)
    {
        if (poff) *poff = 0;
        return iops_dup_default(dir);
    }
    if (strcmp(name, "..") == 0)
    {
        if (poff) *poff = 1;

        ino_t parent_inum = sysfs_get_parent_inode_number(sysfs_dir);
        if (parent_inum == INVALID_INODE)
        {
            // parent has no valid inode in sysfs -> root
            inode_lock(dir->i_sb->imounted_on);
            struct inode *ret =
                VFS_INODE_DIR_LOOKUP(dir->i_sb->imounted_on, "..", NULL);
            inode_unlock(dir->i_sb->imounted_on);
            return ret;
        }
        else
        {
            struct inode *parent = sysfs_find_inode(dir->i_sb, parent_inum);
            DEBUG_EXTRA_PANIC(parent != NULL, "SysFS: Parent inode not found");
            return iops_dup_default(parent);
        }
        return NULL;
    }

    struct sysfs_sb_private *priv =
        (struct sysfs_sb_private *)dir->i_sb->s_fs_info;
    rwspin_read_lock(&priv->lock);

    struct list_head *pos;
    list_for_each(pos, &sysfs_dir->node->child_list)
    {
        struct sysfs_node *node = sysfs_node_from_child_list(pos);
        if (strcmp(node->name, name) == 0)
        {
            struct inode *ip = sysfs_get_inode_from_node(dir->i_sb, node);

            rwspin_read_unlock(&priv->lock);
            return ip;
        }
    }
    rwspin_read_unlock(&priv->lock);

    return NULL;  // not found
}

ssize_t sysfs_iops_get_dirent(struct inode *dir, size_t dir_entry_addr,
                              bool addr_is_userspace, ssize_t seek_pos)
{
    if (!S_ISDIR(dir->i_mode) || seek_pos < 0) return -1;
    struct sysfs_inode *sysfs_dir = sysfs_inode_from_inode(dir);

    struct dirent dir_entry;
    dir_entry.d_off = seek_pos + 1;
    dir_entry.d_reclen = sizeof(struct dirent);

    bool found = false;
    if (seek_pos == 0)
    {
        // "."
        dir_entry.d_ino = dir->inum;
        strncpy(dir_entry.d_name, ".", MAX_DIRENT_NAME);
        found = true;
    }
    else if (seek_pos == 1)
    {
        // ".."
        ino_t parent_inum = sysfs_get_parent_inode_number(sysfs_dir);
        if (parent_inum == INVALID_INODE)
        {
            // root
            dir_entry.d_ino = dir->i_sb->imounted_on->inum;
        }
        else
        {
            dir_entry.d_ino = parent_inum;
        }

        strncpy(dir_entry.d_name, "..", MAX_DIRENT_NAME);
        found = true;
    }
    else
    {
        ssize_t pos_idx = 2;  // skip . and ..
        struct sysfs_sb_private *priv =
            (struct sysfs_sb_private *)dir->i_sb->s_fs_info;
        rwspin_read_lock(&priv->lock);

        struct list_head *pos;
        list_for_each(pos, &sysfs_dir->node->child_list)
        {
            struct sysfs_node *node = sysfs_node_from_child_list(pos);
            if (pos_idx == seek_pos)
            {
                strncpy(dir_entry.d_name, node->name, MAX_DIRENT_NAME);
                found = true;
                break;
            }
            pos_idx++;
        }
        rwspin_read_unlock(&priv->lock);
    }

    if (found == false)
    {
        // end of dir
        return 0;
    }

    dir_entry.d_name[MAX_DIRENT_NAME - 1] = 0;  // ensure null termination
    int32_t res = either_copyout(addr_is_userspace, dir_entry_addr,
                                 (void *)&dir_entry, sizeof(struct dirent));
    if (res < 0) return -EFAULT;

    return seek_pos + 1;
}

ssize_t sysfs_iops_read(struct inode *ip, bool addr_is_userspace, size_t dst,
                        size_t off, size_t n)
{
    // printk("sysfs_fops_read\n");
    if (!S_ISREG(ip->i_mode)) return -1;
    struct sysfs_inode *sysfs_ip = sysfs_inode_from_inode(ip);

    const struct sysfs_ops *sysfs_ops = sysfs_ip->node->kobj->ktype->sysfs_ops;
    if (sysfs_ops == NULL || sysfs_ops->show == NULL ||
        sysfs_ip->node->attribute == NULL)
    {
        return -EINVAL;
    }

    char *dst_buf = kmalloc(n);
    if (dst_buf == NULL) return -ENOMEM;
    memset(dst_buf, 0, n);

    // can't underflow as index 0 is a directory and above is a test for that
    size_t attribute_idx = sysfs_ip->node->sysfs_node_index - 1;
    ssize_t res =
        sysfs_ops->show(sysfs_ip->node->kobj, attribute_idx, dst_buf, n);

    size_t copy_start = off;
    ssize_t copy_len = max(res - off, 0);
    if ((res >= 0) && (copy_len > 0))
    {
        int32_t copy_res = either_copyout(addr_is_userspace, dst,
                                          dst_buf + copy_start, copy_len);
        if (copy_res < 0) return -EFAULT;
    }

    kfree(dst_buf);

    return copy_len;
}

ssize_t sysfs_fops_write(struct file *f, size_t addr, size_t n)
{
    printk("sysfs_fops_write\n");
    return 0;
}