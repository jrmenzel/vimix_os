/* SPDX-License-Identifier: MIT */

// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sys_file.c.

#include <fs/xv6fs/log.h>
#include <fs/xv6fs/xv6fs.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/stat.h>
#include <kernel/string.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

dev_t ROOT_DEVICE_NUMBER;

/// Read the super block.
static void readsb(dev_t dev, struct xv6fs_superblock *sb)
{
    struct buf *bp;

    bp = bio_read(dev, 1);
    memmove(sb, bp->data, sizeof(*sb));
    bio_release(bp);
}

void init_root_file_system(dev_t dev)
{
    readsb(dev, &sb);
    if (sb.magic != XV6FS_MAGIC) panic("invalid file system");
    log_init(dev, &sb);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at block
// sb.inodestart. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a table of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The in-memory
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. inode_alloc() allocates, and inode_put() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in table: an entry in the inode table
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a table entry and increments its ref; inode_put()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   table entry is only correct when ip->valid is 1.
//   inode_lock() reads the inode from
//   the disk and sets ip->valid, while inode_put() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   inode_lock(ip)
//   ... examine and modify ip->xxx ...
//   inode_unlock(ip)
//   inode_put(ip)
//
// inode_lock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// path lookup. iget() increments ip->ref so that the inode
// stays in the table and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The itable.lock spin-lock protects the allocation of itable
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold itable.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

/// In memory inodes, call inode_init() before use.
struct
{
    /// The itable.lock spin-lock protects the allocation of
    /// itable entries. Since ip->ref indicates whether an entry
    /// is free, and ip->dev and ip->inum indicate which i-node
    /// an entry holds, one must hold itable.lock while using any
    /// of those fields.
    struct spinlock lock;
    struct inode inode[MAX_ACTIVE_INODES];
} itable;

void inode_init()
{
    spin_lock_init(&itable.lock, "itable");
    for (size_t i = 0; i < MAX_ACTIVE_INODES; i++)
    {
        sleep_lock_init(&itable.inode[i].lock, "inode");
        itable.inode[i].inum = XV6FS_UNUSED_INODE;
    }
}

struct inode *inode_alloc(dev_t dev, mode_t mode)
{
    return xv6fs_iops_alloc(dev, mode);
}

struct inode *inode_open_or_create(const char *path, mode_t mode, dev_t device)
{
    char name[NAME_MAX];

    struct inode *dir = inode_of_parent_from_path(path, name);
    if (dir == NULL)
    {
        return NULL;
    }

    inode_lock(dir);

    // if the inode already exists, return it
    struct inode *ip = inode_dir_lookup(dir, name, 0);
    if (ip != NULL)
    {
        inode_unlock_put(dir);
        inode_lock(ip);
        if (S_ISREG(mode) &&
            (S_ISREG(ip->i_mode) || S_ISCHR(ip->i_mode) || S_ISBLK(ip->i_mode)))
        {
            return ip;
        }
        inode_unlock_put(ip);
        return NULL;
    }

    // create new inode
    ip = inode_alloc(dir->dev, mode);
    if (ip == NULL)
    {
        inode_unlock_put(dir);
        return NULL;
    }

    inode_lock(ip);
    ip->dev = device;
    ip->nlink = 1;
    inode_update(ip);

#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
    strncpy(ip->path, path, PATH_MAX);
#endif

    if (S_ISDIR(mode))
    {
        // Create . and .. entries.
        // No ip->nlink++ for ".": avoid cyclic ref count.
        if (inode_dir_link(ip, ".", ip->inum) < 0 ||
            inode_dir_link(ip, "..", dir->inum) < 0)
        {
            goto fail;
        }
    }

    if (inode_dir_link(dir, name, ip->inum) < 0)
    {
        goto fail;
    }

    if (S_ISDIR(mode))
    {
        // now that success is guaranteed:
        dir->nlink++;  // for ".."
        inode_update(dir);
    }

    inode_unlock_put(dir);

    return ip;

fail:
    // something went wrong. de-allocate ip.
    ip->nlink = 0;
    inode_update(ip);
    inode_unlock_put(ip);
    inode_unlock_put(dir);
    return NULL;
}

ssize_t inode_open_or_create2(const char *pathname, mode_t mode, dev_t device)
{
    log_begin_fs_transaction();
    struct inode *ip = inode_open_or_create(pathname, mode, device);
    if (ip == NULL)
    {
        log_end_fs_transaction();
        return -1;
    }

    inode_unlock_put(ip);
    log_end_fs_transaction();
    return 0;
}

/// Copy a modified in-memory inode to disk.
/// Must be called after every change to an ip->xxx field
/// that lives on disk.
/// Caller must hold ip->lock.
void inode_update(struct inode *ip) { xv6fs_iops_update(ip); }

struct inode *iget(dev_t dev, uint32_t inum)
{
    spin_lock(&itable.lock);

    // Is the inode already in the table?
    struct inode *empty = NULL;
    struct inode *ip = NULL;
    for (ip = &itable.inode[0]; ip < &itable.inode[MAX_ACTIVE_INODES]; ip++)
    {
        DEBUG_EXTRA_ASSERT(ip != NULL, "invalid inode");

        if (ip->ref > 0 && ip->i_sb_dev == dev && ip->inum == inum)
        {
            ip->ref++;
            spin_unlock(&itable.lock);
            return ip;
        }

        if (empty == NULL && ip->ref == 0)  // Remember empty slot.
        {
            empty = ip;
        }
    }

    // Recycle an inode entry.
    if (empty == NULL)
    {
        panic("iget: no inodes left. See MAX_ACTIVE_INODES.");
    }

    ip = empty;
    ip->i_sb_dev = dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0;
    spin_unlock(&itable.lock);

    return ip;
}

struct inode *inode_dup(struct inode *ip)
{
    spin_lock(&itable.lock);
    ip->ref++;
    spin_unlock(&itable.lock);
    return ip;
}

void inode_lock(struct inode *ip)
{
    if (ip == NULL)
    {
        panic("inode_lock: inode is NULL");
    }
    if (ip->ref < 1)
    {
        panic("inode_lock: inode has an invalid reference count");
    }

    sleep_lock(&ip->lock);

    if (ip->valid == 0)
    {
        xv6fs_iops_read_in(ip);
        ip->valid = 1;
        if (INODE_HAS_TYPE(ip->i_mode) == false)
        {
            panic("inode_lock: inode has no type");
        }
    }
}

void inode_unlock(struct inode *ip)
{
#ifdef CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
    if (ip == NULL)
    {
        panic("inode_unlock failed: inode is NULL");
    }
    if (ip->ref < 1)
    {
        panic("inode_unlock failed: reference count invalid");
    }
#endif  // CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
#ifdef CONFIG_DEBUG_SLEEPLOCK
    if (!sleep_lock_is_held_by_this_cpu(&ip->lock))
    {
        panic("inode_unlock failed: sleeplock not held by this CPU");
    }
#endif  // CONFIG_DEBUG_SLEEPLOCK

    sleep_unlock(&ip->lock);
}

void inode_put(struct inode *ip)
{
    spin_lock(&itable.lock);

    if (ip->ref == 1 && ip->valid && ip->nlink == 0)
    {
        // inode has no links and no other references: truncate and free.

        // ip->ref == 1 means no other process can have ip locked,
        // so this sleep_lock() won't block (or deadlock).
        sleep_lock(&ip->lock);

        spin_unlock(&itable.lock);

        inode_trunc(ip);
        ip->i_mode = 0;
        inode_update(ip);
        ip->valid = 0;

        sleep_unlock(&ip->lock);

        spin_lock(&itable.lock);
    }

    ip->ref--;
    spin_unlock(&itable.lock);
}

void inode_unlock_put(struct inode *ip)
{
    inode_unlock(ip);
    inode_put(ip);
}

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

/// Truncate inode (discard contents).
/// Caller must hold ip->lock.
void inode_trunc(struct inode *ip)
{
    xv6fs_iops_trunc(ip);

    ip->size = 0;
    inode_update(ip);
}

void inode_stat(struct inode *ip, struct stat *st)
{
    st->st_dev = ip->i_sb_dev;
    st->st_rdev = ip->dev;
    st->st_ino = ip->inum;
    st->st_mode = ip->i_mode;
    st->st_nlink = ip->nlink;
    st->st_size = ip->size;
}

ssize_t inode_read(struct inode *ip, bool addr_is_userspace, size_t dst,
                   size_t off, size_t n)
{
    if (off > ip->size || off + n < off)
    {
        return 0;
    }
    if (off + n > ip->size)
    {
        n = ip->size - off;
    }

    ssize_t m = 0;
    ssize_t tot = 0;
    for (tot = 0; tot < n; tot += m, off += m, dst += m)
    {
        size_t addr = bmap(ip, off / BLOCK_SIZE);
        if (addr == 0)
        {
            break;
        }
        struct buf *bp = bio_read(ip->dev, addr);
        m = min(n - tot, BLOCK_SIZE - off % BLOCK_SIZE);

        if (either_copyout(addr_is_userspace, dst,
                           bp->data + (off % BLOCK_SIZE), m) == -1)
        {
            bio_release(bp);
            tot = -1;
            break;
        }
        bio_release(bp);
    }
    return tot;
}

ssize_t inode_write(struct inode *ip, bool src_addr_is_userspace, size_t src,
                    size_t off, size_t n)
{
    if (off > ip->size || off + n < off)
    {
        return -1;
    }
    if (off + n > MAXFILE * BLOCK_SIZE)
    {
        return -1;
    }

    ssize_t m = 0;
    ssize_t tot;
    for (tot = 0; tot < n; tot += m, off += m, src += m)
    {
        size_t addr = bmap(ip, off / BLOCK_SIZE);
        if (addr == 0)
        {
            break;
        }
        struct buf *bp = bio_read(ip->dev, addr);
        m = min(n - tot, BLOCK_SIZE - off % BLOCK_SIZE);

        if (either_copyin(bp->data + (off % BLOCK_SIZE), src_addr_is_userspace,
                          src, m) == -1)
        {
            bio_release(bp);
            break;
        }
        log_write(bp);
        bio_release(bp);
    }

    if (off > ip->size)
    {
        ip->size = off;
    }

    // write the i-node back to disk even if the size didn't change
    // because the loop above might have called bmap() and added a new
    // block to ip->addrs[].
    inode_update(ip);

    return tot;
}

// Directories

int file_name_cmp(const char *s, const char *t)
{
    return strncmp(s, t, NAME_MAX);
}

struct inode *inode_dir_lookup(struct inode *dir, const char *name,
                               uint32_t *poff)
{
    struct xv6fs_dirent de;

    if (!S_ISDIR(dir->i_mode))
    {
        panic("inode_dir_lookup dir parameter is not a DIR!");
    }

    for (size_t off = 0; off < dir->size; off += sizeof(de))
    {
        if (inode_read(dir, false, (size_t)&de, off, sizeof(de)) != sizeof(de))
        {
            panic("inode_dir_lookup read error");
        }
        if (de.inum == XV6FS_UNUSED_INODE)
        {
            continue;
        }

        if (file_name_cmp(name, de.name) == 0)
        {
            // entry matches path element
            if (poff)
            {
                *poff = off;
            }
            uint32_t inum = de.inum;
            dev_t dev = dir->i_sb_dev;
            return iget(dev, inum);
        }
    }

    return NULL;
}

int inode_dir_link(struct inode *dir, char *name, uint32_t inum)
{
    // Check that name is not present.
    struct inode *ip = inode_dir_lookup(dir, name, 0);
    if (ip != NULL)
    {
        inode_put(ip);
        return -1;
    }

    // Look for an empty xv6fs_dirent.
    struct xv6fs_dirent de;
    size_t off;
    for (off = 0; off < dir->size; off += sizeof(de))
    {
        ssize_t read = inode_read(dir, false, (size_t)&de, off, sizeof(de));
        if (read != sizeof(de))
        {
            panic("inode_dir_link read wrong amount of data");
        }
        if (de.inum == XV6FS_UNUSED_INODE)
        {
            break;
        }
    }

    strncpy(de.name, name, XV6_NAME_MAX);
    de.inum = inum;

    ssize_t written = inode_write(dir, false, (size_t)&de, off, sizeof(de));
    if (written != sizeof(de))
    {
        return -1;
    }

    return 0;
}

ssize_t inode_get_dirent(struct inode *dir, size_t dir_entry_addr,
                         bool addr_is_userspace, ssize_t seek_pos)
{
    if (!S_ISDIR(dir->i_mode) || seek_pos < 0) return -1;

    struct xv6fs_dirent xv6_dir_entry;
    inode_lock(dir);
    ssize_t new_seek_pos = seek_pos;

    do {
        size_t read_bytes;
        read_bytes =
            inode_read(dir, false, (size_t)&xv6_dir_entry, (size_t)new_seek_pos,
                       sizeof(struct xv6fs_dirent));
        if (read_bytes <= 0)
        {
            inode_unlock(dir);
            return read_bytes;  // 0 if no more dirents to read or -1 on error
        }
        else if (read_bytes < sizeof(struct xv6fs_dirent))
        {
            inode_unlock(dir);
            return 0;
        }
        new_seek_pos += read_bytes;
    } while (xv6_dir_entry.inum == XV6FS_UNUSED_INODE);  // skip unused entries

    inode_unlock(dir);

    struct dirent dir_entry;
    dir_entry.d_ino = xv6_dir_entry.inum;
    strncpy(dir_entry.d_name, xv6_dir_entry.name, XV6_NAME_MAX);
    dir_entry.d_off = (long)(new_seek_pos);

    int32_t res = either_copyout(addr_is_userspace, dir_entry_addr,
                                 (void *)&dir_entry, sizeof(struct dirent));
    if (res < 0) return -1;

    return new_seek_pos;
}

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
static const char *skipelem(const char *path, char *name)
{
    while (*path == '/')
    {
        path++;
    }
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
    if (len >= NAME_MAX)
    {
        memmove(name, s, NAME_MAX);
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

/// Look up and return the inode for a path name.
/// If get_parent == true, return the inode for the parent and copy the final
/// path element into name, which must have room for NAME_MAX bytes.
/// Must be called inside a transaction since it calls inode_put().
static struct inode *namex(const char *path, bool get_parent, char *name)
{
    struct inode *ip;

    if (*path == '/')
    {
        ip = iget(ROOT_DEVICE_NUMBER, ROOT_INODE);
    }
    else
    {
        ip = inode_dup(get_current()->cwd);
    }

    while ((path = skipelem(path, name)) != 0)
    {
        inode_lock(ip);

        if (INODE_HAS_TYPE(ip->i_mode) == false)
        {
            panic("inode_lock: inode has no type");
        }

        if (!S_ISDIR(ip->i_mode))
        {
            inode_unlock_put(ip);
            return NULL;
        }
        if (get_parent && *path == '\0')
        {
            // Stop one level early.
            inode_unlock(ip);
            return ip;
        }
        struct inode *next = inode_dir_lookup(ip, name, NULL);
        if (next == NULL)
        {
            inode_unlock_put(ip);
            return NULL;
        }
        inode_unlock_put(ip);
        ip = next;
    }
    if (get_parent)
    {
        inode_put(ip);
        return NULL;
    }
    return ip;
}

struct inode *inode_from_path(const char *path)
{
    char name[NAME_MAX];
    return namex(path, false, name);
}

struct inode *inode_of_parent_from_path(const char *path, char *name)
{
    return namex(path, true, name);
}
