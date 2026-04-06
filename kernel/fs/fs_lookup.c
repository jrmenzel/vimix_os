/* SPDX-License-Identifier: MIT */

#include <fs/dentry_cache.h>
#include <fs/fs_lookup.h>
#include <fs/vfs.h>
#include <kernel/errno.h>
#include <kernel/permission.h>
#include <kernel/proc.h>
#include <kernel/string.h>

// Paths

/// Copy the next path element from path into name.
/// Return a pointer to the element following the copied one.
/// The returned path has no leading slashes,
/// so the caller can check *path=='\0' to see if the name is the last one.
/// If no name to remove, return 0.
///
/// Examples:
///   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
///   skipelem("///a//bb", name) = "bb", setting name = "a"
///   skipelem("a", name) = "", setting name = "a"
///   skipelem("", name) = skipelem("////", name) = 0
static const char *skipelem(const char *path, char *name, syserr_t *error)
{
    // skip leading slashes and './' like "./a/b" -> "a/b" or "///a/b" -> "a/b"
    bool skipped = false;
    do
    {
        skipped = false;
        if ((*path == '.') && (*(path + 1) == '/'))
        {
            skipped = true;
            path += 2;
        }
        else if (*path == '/')
        {
            skipped = true;
            path++;
        }
    } while (skipped);

    if (*path == 0)
    {
        return NULL;
    }

    const char *s = path;
    while (*path != '/' && *path != 0)
    {
        path++;
    }

    size_t len = path - s;
    if (len > NAME_MAX)
    {
        *error = -ENOENT;
        return path;
    }
    else
    {
        memmove(name, s, len);
        name[len] = 0;
    }

    while (*path == '/')
    {
        path++;
    }
    return path;
}

struct dentry *dentry_from_path(const char *path, syserr_t *error)
{
    DEBUG_EXTRA_PANIC(path != NULL, "dentry_from_path: path is NULL");
    DEBUG_EXTRA_PANIC(error != NULL, "dentry_from_path: error is NULL");

    struct process *proc = get_current();
    struct dentry *dp = NULL;
    if (*path == '/')
    {
        dp = dentry_cache_get_root();
    }
    else if (*path == 0)
    {
        // path "" is invalid
        *error = -EINVAL;
        return NULL;
    }
    else
    {
        dp = dentry_get(proc->cwd_dentry);
        dentry_lock(dp);
        if (dentry_is_unlinked(dp))
        {
            dentry_unlock_put(dp);
            *error = -ENOENT;
            return NULL;
        }
        dentry_unlock(dp);
    }

    *error = 0;
    char name[NAME_MAX];
    while ((path = skipelem(path, name, error)) != NULL)
    {
        if (*error != 0)
        {
            dentry_put(dp);
            return NULL;
        }

        // Invalid dentries are allowed (and might get returned) to indicate
        // non-existing files, but if assumed to be a directory (aka not the
        // last path component) it's an error.
        if (dentry_is_invalid(dp))
        {
            dentry_put(dp);
            *error = -ENOENT;
            return NULL;
        }

        if (!S_ISDIR(dp->ip->i_mode))
        {
            dentry_put(dp);
            *error = -ENOTDIR;
            return NULL;
        }

        if (check_dentry_permission(get_current(), dp, MAY_EXEC) < 0)
        {
            dentry_put(dp);
            *error = -EACCES;
            return NULL;
        }

        // we are in a (by this process) traversable and existing dir, now look
        // up the next path element
        struct dentry *next;
        if (name[0] == '.' && name[1] == '\0')
        {
            // current dir
            next = dentry_get(dp);
        }
        else if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
        {
            // parent dir
            if (dp->parent == NULL)
            {
                // already at root
                next = dentry_get(dp);
            }
            else
            {
                next = dentry_get(dp->parent);
            }
        }
        else
        {
            next = dentry_cache_lookup(dp, name);
            if (next == NULL)
            {
                next = dentry_alloc_init_orphan(name, NULL);
                if (next == NULL)
                {
                    panic("dentry_from_path: out of memory");
                }
                inode_lock_exclusive(dp->ip);
                next = VFS_INODE_LOOKUP(dp->ip, next);
                dentry_lock(dp);
                struct dentry *created_concurrently =
                    dentry_cache_lookup_locked(dp, name);
                if (created_concurrently != NULL)
                {
                    // another thread created the dentry concurrently, use it
                    dentry_unlock(dp);
                    dentry_put(next);
                    next = created_concurrently;
                }
                else
                {
                    // register the newly created dentry
                    dentry_register_with_parent(dp, next);
                    dentry_unlock(dp);
                }
                inode_unlock_exclusive(dp->ip);
            }
        }

        dentry_put(dp);
        dp = next;
    }

    return dp;
}
