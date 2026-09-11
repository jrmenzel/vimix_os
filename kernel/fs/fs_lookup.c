/* SPDX-License-Identifier: MIT */

#include <fs/dentry_cache.h>
#include <fs/fs_lookup.h>
#include <fs/vfs.h>
#include <kernel/errno.h>
#include <kernel/permission.h>
#include <kernel/proc.h>
#include <kernel/string.h>

// Paths

/*
 * Keep link expansion bounded even when every expanded path fits PATH_MAX.
 * This is deliberately independent of the path-length limit: a/b -> c/d ->
 * a/b is otherwise an infinite walk.
 */
#define MAX_SYMLINK_FOLLOWS 40

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
        *error = -ENAMETOOLONG;
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

struct dentry *dentry_from_path_mode(const char *path,
                                     enum Lookup_Mode lookup_mode,
                                     syserr_t *error)
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
        dcache_read_lock();
        if (dentry_is_unlinked(dp))
        {
            dcache_read_unlock();
            dentry_put(dp);
            *error = -ENOENT;
            return NULL;
        }
        dcache_read_unlock();
    }

    return dentry_from_path_at(dp, path, lookup_mode, error);
}

struct dentry *dentry_from_path_at(struct dentry *dp, const char *path,
                                   enum Lookup_Mode lookup_mode,
                                   syserr_t *error)
{
    DEBUG_EXTRA_PANIC(dp != NULL, "dentry_from_path_at: dp is NULL");
    DEBUG_EXTRA_PANIC(path != NULL, "dentry_from_path_at: path is NULL");
    DEBUG_EXTRA_PANIC(error != NULL, "dentry_from_path_at: error is NULL");

    *error = 0;

    // copy the path for link lookup
    char pending_path[PATH_MAX];
    size_t path_length = strnlen(path, sizeof(pending_path));
    if (path_length == sizeof(pending_path))
    {
        dentry_put(dp);
        *error = -ENAMETOOLONG;
        return NULL;
    }
    memmove(pending_path, path, path_length + 1);

    const char *remaining_path = pending_path;
    size_t followed_links =
        0;  // to limit lookup / prevent recursion in the path
    char name[NAME_MAX + 1];
    while (true)
    {
        remaining_path = skipelem(remaining_path, name, error);
        if (remaining_path == NULL) break;

        if (*error != 0)
        {
            dentry_put(dp);
            return NULL;
        }

        dcache_read_lock();

        // Invalid dentries are allowed (and might get returned) to indicate
        // non-existing files, but if assumed to be a directory (aka not the
        // last path component) it's an error.
        if (dentry_is_invalid(dp))
        {
            dcache_read_unlock();
            dentry_put(dp);
            *error = -ENOENT;
            return NULL;
        }

        if (!S_ISDIR(dp->ip->i_mode))
        {
            dcache_read_unlock();
            dentry_put(dp);
            *error = -ENOTDIR;
            return NULL;
        }
        dcache_read_unlock();

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
            dcache_read_lock();
            if (dp->parent == NULL)
            {
                // already at root
                next = dentry_get(dp);
            }
            else
            {
                next = dentry_get(dp->parent);
            }
            dcache_read_unlock();
        }
        else
        {
            dcache_read_lock();
            next = dentry_cache_lookup_tree_locked(dp, name);
            dcache_read_unlock();
            if (next == NULL)
            {
                struct dentry *lookup_result =
                    dentry_alloc_init_orphan(name, NULL);
                if (lookup_result == NULL)
                {
                    panic("dentry_from_path: out of memory");
                }

                inode_lock_exclusive(dp->ip);
                lookup_result = VFS_INODE_LOOKUP(dp->ip, lookup_result);
                inode_unlock_exclusive(dp->ip);

                dcache_write_lock();
                struct dentry *created_concurrently =
                    dentry_cache_lookup_tree_locked(dp, name);
                if (created_concurrently != NULL)
                {
                    // another thread created the dentry concurrently, use it
                    dcache_write_unlock();

                    if (lookup_result != NULL)
                    {
                        dentry_put(lookup_result);
                    }
                    next = created_concurrently;
                }
                else
                {
                    // register the newly created dentry
                    dentry_register_with_parent(dp, lookup_result);
                    dcache_write_unlock();

                    next = lookup_result;
                }
            }
        }

        // follow symlinks
        bool final_component = *remaining_path == '\0';
        if (dentry_is_valid(next) && S_ISLNK(next->ip->i_mode) &&
            (!final_component || lookup_mode == FOLLOW_FINAL_SYMLINK))
        {
            if (followed_links++ == MAX_SYMLINK_FOLLOWS)
            {
                dentry_put(next);
                dentry_put(dp);
                *error = -ELOOP;
                return NULL;
            }

            // A symlink target is interpreted relative to the symlink's
            // parent directory, not relative to the original dp (CWD or root).
            // Retain that directory before replacing the current dentry.
            struct dentry *link_parent = dentry_get(dp);
            char target[PATH_MAX];
            inode_lock(next->ip);
            size_t target_length = next->ip->size;
            syserr_t bytes_read = 0;
            if (target_length != 0 && target_length < PATH_MAX)
            {
                bytes_read = VFS_INODE_READ_KERNEL(next->ip, 0, (size_t)target,
                                                   target_length);
            }
            inode_unlock(next->ip);
            if ((target_length == 0) || (target_length >= PATH_MAX))
            {
                dentry_put(link_parent);
                dentry_put(next);
                dentry_put(dp);
                *error = target_length == 0 ? -ENOENT : -ENAMETOOLONG;
                return NULL;
            }
            if ((bytes_read < 0) || ((size_t)bytes_read != target_length))
            {
                dentry_put(link_parent);
                dentry_put(next);
                dentry_put(dp);
                *error = bytes_read < 0 ? bytes_read : -EIO;
                return NULL;
            }
            target[target_length] = '\0';

            size_t remainder_length = strlen(remaining_path);
            size_t separator_length = remainder_length == 0 ? 0 : 1;
            if (target_length + separator_length + remainder_length >=
                sizeof(pending_path))
            {
                dentry_put(link_parent);
                dentry_put(next);
                dentry_put(dp);
                *error = -ENAMETOOLONG;
                return NULL;
            }

            // Use a separate buffer: remaining_path points into pending_path.
            char expanded_path[PATH_MAX];
            memmove(expanded_path, target, target_length);
            if (separator_length != 0)
            {
                expanded_path[target_length] = '/';
            }
            memmove(expanded_path + target_length + separator_length,
                    remaining_path, remainder_length + 1);
            memmove(pending_path, expanded_path,
                    target_length + separator_length + remainder_length + 1);

            dentry_put(next);
            dentry_put(dp);
            if (target[0] == '/')
            {
                dentry_put(link_parent);
                dp = dentry_cache_get_root();
            }
            else
            {
                dp = link_parent;
            }
            remaining_path = pending_path;
            continue;
        }

        dentry_put(dp);
        dp = next;
    }

    return dp;
}
