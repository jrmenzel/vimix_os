/* SPDX-License-Identifier: MIT */

//
// link / unlink / rmdir syscalls
//

#include <drivers/device.h>
#include <fs/dentry_cache.h>
#include <fs/fs_lookup.h>
#include <kernel/dirent.h>
#include <kernel/fcntl.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/permission.h>
#include <kernel/proc.h>
#include <kernel/stat.h>
#include <kernel/string.h>
#include <syscalls/syscall.h>

syserr_t do_link(char *path_from, char *path_to);

syserr_t do_rename(char *old_path, char *new_path);

syserr_t do_rm(char *path, bool is_rmdir);

/// @brief Syscall unlink
/// @param path path name
/// @return 0 on success, -errno on error
static inline syserr_t do_unlink(char *path) { return do_rm(path, false); }

/// @brief Syscall rmdir
/// @param path path name
/// @return 0 on success, -errno on error
static inline syserr_t do_rmdir(char *path) { return do_rm(path, true); }

syserr_t sys_link()
{
    // parameter 0 / 1: const char *from / *to
    char path_to[PATH_MAX], path_from[PATH_MAX];
    if (argstr(0, path_from, PATH_MAX) < 0 || argstr(1, path_to, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    return do_link(path_from, path_to);
}

syserr_t sys_rename()
{
    char old_path[PATH_MAX], new_path[PATH_MAX];
    if (argstr(0, old_path, PATH_MAX) < 0 || argstr(1, new_path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    return do_rename(old_path, new_path);
}

syserr_t sys_unlink()
{
    // parameter 0: const char *pathname
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    syserr_t ret = do_unlink(path);
    return ret;
}

syserr_t sys_rmdir()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    return do_rmdir(path);
}

/// @brief creates a new hard link for file path_from with
/// new name path_to.
/// @return 0 on success, -errno on error
syserr_t do_link(char *path_from, char *path_to)
{
    syserr_t error = 0;
    struct dentry *dentry_from = dentry_from_path(path_from, &error);
    if (dentry_from == NULL)
    {
        return error;
    }
    if (dentry_is_invalid(dentry_from))
    {
        dentry_put(dentry_from);
        return -ENOENT;
    }

    if (S_ISDIR(dentry_from->ip->i_mode))
    {
        dentry_put(dentry_from);
        return -EISDIR;
    }

    struct dentry *dentry_to = dentry_from_path(path_to, &error);
    if (dentry_to == NULL)
    {
        dentry_put(dentry_from);
        return -ENOENT;
    }

    if (dentry_is_valid(dentry_to))
    {
        dentry_put(dentry_to);
        dentry_put(dentry_from);
        return -EEXIST;
    }

    dcache_read_lock();
    struct dentry *dir_to = dentry_get(dentry_to->parent);
    dcache_read_unlock();
    syserr_t perm_ok =
        check_dentry_permission(get_current(), dir_to, MAY_WRITE);
    if (perm_ok < 0)
    {
        dentry_put(dir_to);
        dentry_put(dentry_to);
        dentry_put(dentry_from);
        return perm_ok;
    }

    if (dentry_from->ip->dev != dir_to->ip->dev)
    {
        dentry_put(dir_to);
        dentry_put(dentry_to);
        dentry_put(dentry_from);
        return -EOTHER;
    }

    inode_lock_exclusive_2(dir_to->ip, dentry_from->ip);
    syserr_t ret = VFS_INODE_LINK(dentry_from, dir_to->ip, dentry_to);
    inode_unlock_exclusive_2(dir_to->ip, dentry_from->ip);
    dentry_put(dir_to);
    dentry_put(dentry_to);
    dentry_put(dentry_from);

    return ret;
}

static bool path_ends_in_dot_or_dotdot(const char *path)
{
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') len--;

    size_t start = len;
    while (start > 0 && path[start - 1] != '/') start--;
    size_t component_len = len - start;

    bool ends_in_dot = (component_len == 1) && (path[start] == '.');
    bool ends_in_dot_dot = (component_len == 2) && (path[start] == '.') &&
                           (path[start + 1] == '.');

    return (ends_in_dot || ends_in_dot_dot);
}

// helper to reduce duplicated code
static inline void dentry_put_4(struct dentry *a, struct dentry *b,
                                struct dentry *c, struct dentry *d)
{
    dentry_put(a);
    dentry_put(b);
    dentry_put(c);
    dentry_put(d);
}

/// @brief Atomically rename an object within one mounted file system.
syserr_t do_rename(char *old_path, char *new_path)
{
    if (path_ends_in_dot_or_dotdot(old_path) ||
        path_ends_in_dot_or_dotdot(new_path))
    {
        return -EINVAL;
    }

    syserr_t ret = 0;
    struct dentry *old_dentry = dentry_from_path(old_path, &ret);
    if (old_dentry == NULL) return ret;
    if (dentry_is_invalid(old_dentry))
    {
        dentry_put(old_dentry);
        return -ENOENT;
    }

    struct dentry *new_dentry = dentry_from_path(new_path, &ret);
    if (new_dentry == NULL)
    {
        dentry_put(old_dentry);
        return ret;
    }

    dcache_read_lock();
    struct dentry *old_parent =
        old_dentry->parent == NULL ? NULL : dentry_get(old_dentry->parent);
    struct dentry *new_parent =
        new_dentry->parent == NULL ? NULL : dentry_get(new_dentry->parent);
    dcache_read_unlock();

    if ((old_parent == NULL) || (new_parent == NULL))
    {
        if (new_parent != NULL) dentry_put(new_parent);
        if (old_parent != NULL) dentry_put(old_parent);
        dentry_put(new_dentry);
        dentry_put(old_dentry);
        return -EINVAL;
    }

    struct inode *old_parent_ip = dentry_inode(old_parent);
    struct inode *new_parent_ip = dentry_inode(new_parent);
    struct inode *old_ip = dentry_inode(old_dentry);
    struct inode *new_ip = dentry_inode(new_dentry);

    if ((old_parent_ip == NULL) || (new_parent_ip == NULL) || (old_ip == NULL))
    {
        dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
        return -ENOENT;
    }
    if (old_parent_ip->i_sb != new_parent_ip->i_sb)
    {
        dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
        return -EXDEV;
    }
    // A mounted root is not an entry in its visible parent's backing file
    // system, whether it is used as the source or destination.
    if (old_ip->i_sb != old_parent_ip->i_sb)
    {
        dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
        return -EXDEV;
    }
    if (new_ip != NULL && new_ip->i_sb != new_parent_ip->i_sb)
    {
        dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
        return -EXDEV;
    }

    ret = check_dentry_permission(get_current(), old_parent, MAY_UNLINK);
    if (ret < 0)
    {
        dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
        return ret;
    }
    if (new_parent != old_parent)
    {
        ret = check_dentry_permission(get_current(), new_parent, MAY_UNLINK);
        if (ret < 0)
        {
            dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
            return ret;
        }
    }

    bool old_is_dir = S_ISDIR(old_ip->i_mode);
    if (new_ip != NULL)
    {
        bool new_is_dir = S_ISDIR(new_ip->i_mode);
        if (old_is_dir && !new_is_dir)
        {
            dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
            return -ENOTDIR;
        }
        if (!old_is_dir && new_is_dir)
        {
            dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
            return -EISDIR;
        }
    }

    // POSIX specifies success when both names already identify one inode.
    if (new_ip == old_ip)
    {
        dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
        return 0;
    }

    if (old_is_dir)
    {
        dcache_read_lock();
        for (struct dentry *ancestor = new_parent; ancestor != NULL;
             ancestor = ancestor->parent)
        {
            if (ancestor == old_dentry)
            {
                ret = -EINVAL;
                break;
            }
        }
        dcache_read_unlock();
        if (ret < 0)
        {
            dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
            return ret;
        }
    }

    // This dentry donates its allocated new name to old_dentry after the
    // backend succeeds, while retaining the old name as a negative cache entry.
    struct dentry *old_name_placeholder =
        dentry_alloc_init_orphan(new_dentry->name, NULL);
    if (old_name_placeholder == NULL)
    {
        dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
        return -ENOMEM;
    }

    inode_lock_exclusive_2_safe(old_parent_ip, new_parent_ip);

    dcache_read_lock();
    bool lookup_is_current = old_dentry->parent == old_parent &&
                             new_dentry->parent == new_parent &&
                             dentry_inode(old_dentry) == old_ip &&
                             dentry_inode(new_dentry) == new_ip;
    dcache_read_unlock();
    if (!lookup_is_current)
    {
        inode_unlock_exclusive_2_save(old_parent_ip, new_parent_ip);
        dentry_put(old_name_placeholder);
        dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);
        return -ENOENT;
    }

    ret =
        VFS_INODE_RENAME(old_parent_ip, old_dentry, new_parent_ip, new_dentry);
    if (ret == 0)
    {
        dcache_write_lock();
        const char *old_name = old_dentry->name;
        old_dentry->name = old_name_placeholder->name;
        old_name_placeholder->name = old_name;

        struct dentry *old_dentry_parent =
            dentry_unregister_from_parent(old_dentry);
        struct dentry *new_dentry_parent =
            dentry_unregister_from_parent(new_dentry);
        dentry_register_with_parent(new_parent, old_dentry);
        dentry_register_with_parent(old_parent, old_name_placeholder);
        if (new_ip != NULL)
        {
            dentry_register_with_parent(g_dentry_cache.unlinked_root,
                                        new_dentry);
        }
        dcache_write_unlock();

        dentry_put(new_dentry_parent);
        dentry_put(old_dentry_parent);
    }

    inode_unlock_exclusive_2_save(old_parent_ip, new_parent_ip);
    dentry_put(old_name_placeholder);
    dentry_put_4(new_parent, old_parent, new_dentry, old_dentry);

    return ret;
}

syserr_t do_rm(char *path, bool is_rmdir)
{
    syserr_t error = 0;
    struct dentry *file = dentry_from_path(path, &error);

    if (file == NULL)
    {
        return error;
    }

    if (dentry_is_invalid(file))
    {
        dentry_put(file);
        return -ENOENT;
    }

    if (is_rmdir && !S_ISDIR(file->ip->i_mode))
    {
        dentry_put(file);
        return -ENOTDIR;
    }
    else if (!is_rmdir && S_ISDIR(file->ip->i_mode))
    {
        dentry_put(file);
        return -EISDIR;
    }

    dcache_read_lock();
    bool file_is_unlinked = dentry_is_unlinked(file);
    struct dentry *parent =
        file->parent == NULL ? NULL : dentry_get(file->parent);
    dcache_read_unlock();

    // The target can be concurrently moved to the internal unlinked tree.
    // In that case parent->ip is NULL and must not be locked.
    if (file_is_unlinked || file->ip == NULL || parent == NULL ||
        parent->ip == NULL)
    {
        dentry_put(file);
        dentry_put(parent);
        return -ENOENT;
    }

    size_t name_len = strlen(path);
    bool cant_unlink = false;

    if (file_name_cmp(file->name, ".") == 0 ||
        file_name_cmp(file->name, "..") == 0)
    {
        cant_unlink = true;
    }

    if (name_len >= 2)
    {
        if ((path[name_len - 1] == '.') && (path[name_len - 2] == '/'))
        {
            // path end in /.
            cant_unlink = true;
        }
        else if (name_len >= 3)
        {
            if ((path[name_len - 1] == '.') && (path[name_len - 2] == '.') &&
                (path[name_len - 3] == '/'))
            {
                // path ends in /..
                cant_unlink = true;
            }
        }
    }

    if (cant_unlink)
    {
        dentry_put(file);
        dentry_put(parent);
        return -EPERM;
    }

    // Cannot unlink "." or ".." or "/".
    // if (file_name_cmp(file->name, ".") == 0 ||
    //    file_name_cmp(file->name, "..") == 0 || parent == NULL)
    //{
    //    dentry_put(file);
    //    dentry_put(parent);
    //    return -EPERM;
    //}

    // need write permission in directory where file gets unlinked
    syserr_t perm_ok =
        check_dentry_permission(get_current(), parent, MAY_UNLINK);
    if (perm_ok < 0)
    {
        dentry_put(file);
        dentry_put(parent);
        return perm_ok;
    }

    // Snapshot inode pointers once and keep using those pointers for
    // lock/unlock. The dentry cache linkage can change concurrently.
    struct inode *parent_ip = parent->ip;
    struct inode *file_ip = file->ip;

    if (parent_ip == NULL || file_ip == NULL)
    {
        dentry_put(file);
        dentry_put(parent);
        return -ENOENT;
    }

    // Keep both inodes alive across backend dispatch, independent of dentry
    // cache rewiring.
    inode_get(parent_ip);
    inode_get(file_ip);

    inode_lock_exclusive_2(parent_ip, file_ip);
    dcache_read_lock();
    if (file->parent != parent)
    {
        // file got moved in the meantime -> abort
        dcache_read_unlock();
        inode_unlock_exclusive_2(parent_ip, file_ip);
        inode_put(file_ip);
        inode_put(parent_ip);
        dentry_put(file);
        dentry_put(parent);
        return -EACCES;
    }
    dcache_read_unlock();

    struct dentry *new_dentry = dentry_alloc_init_orphan(file->name, NULL);
    if (new_dentry == NULL)
    {
        inode_unlock_exclusive_2(parent_ip, file_ip);
        inode_put(file_ip);
        inode_put(parent_ip);
        dentry_put(file);
        dentry_put(parent);
        return -ENOMEM;
    }

    // switch the lookup to the new dentry to prevent further access
    dcache_write_lock();
    // The dentry linkage can still mutate between lock handoff windows.
    // Treat this as a stale lookup race, not a kernel-fatal condition.
    if (file->parent != parent || file->ip != file_ip ||
        parent->ip != parent_ip || new_dentry->parent != NULL ||
        file_name_cmp(file->name, new_dentry->name) != 0)
    {
        dcache_write_unlock();
        inode_unlock_exclusive_2(parent_ip, file_ip);
        inode_put(file_ip);
        inode_put(parent_ip);
        dentry_put(file);
        dentry_put(parent);
        dentry_put(new_dentry);
        return -ENOENT;
    }

    struct dentry *file_old_parent = dentry_unregister_from_parent(file);
    dentry_register_with_parent(g_dentry_cache.unlinked_root, file);
    dentry_register_with_parent(parent, new_dentry);
    dcache_write_unlock();
    dentry_put(file_old_parent);

    syserr_t (*rm_backend)(struct inode *, struct dentry *) =
        is_rmdir ? parent_ip->i_sb->i_op->iops_rmdir
                 : parent_ip->i_sb->i_op->iops_unlink;
    DEBUG_EXTRA_PANIC(rm_backend != NULL,
                      "do_rm: selected backend rm operation is NULL");

    syserr_t ret = rm_backend(parent_ip, file);
    if (ret < 0)
    {
        // something went wrong, re-add to parent's list
        dcache_write_lock();
        struct dentry *new_dentry_old_parent =
            dentry_unregister_from_parent(new_dentry);
        struct dentry *unlinked_old_parent =
            dentry_unregister_from_parent(file);
        dentry_register_with_parent(parent, file);
        dcache_write_unlock();
        dentry_put(new_dentry_old_parent);
        dentry_put(unlinked_old_parent);
    }
    else
    {
        // success and dentry "file" can not get discovered anymore
        if (kref_read(&file->ref) == 1)
        {
            // no other references, free immediately
            dentry_cache_remove_from_unlinked(file);
        }
    }
    inode_unlock_exclusive_2(parent_ip, file_ip);
    inode_put(file_ip);
    inode_put(parent_ip);

    dentry_put(file);
    dentry_put(parent);
    dentry_put(new_dentry);

    return ret;
}
