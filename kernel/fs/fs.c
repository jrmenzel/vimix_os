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
// are in sysfile.c.

#include <fs/xv6fs/log.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/stat.h>
#include <kernel/string.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
/// there should be one xv6fs_superblock per disk device, but we run with
/// only one device
struct xv6fs_superblock sb;

/// Read the super block.
static void readsb(int dev, struct xv6fs_superblock *sb)
{
    struct buf *bp;

    bp = bio_read(dev, 1);
    memmove(sb, bp->data, sizeof(*sb));
    bio_release(bp);
}

/// Init fs
void init_root_file_system(int dev)
{
    readsb(dev, &sb);
    if (sb.magic != XV6FS_MAGIC) panic("invalid file system");
    log_init(dev, &sb);
}

/// Zero a block.
static void block_zero(int dev, int bno)
{
    struct buf *bp;

    bp = bio_read(dev, bno);
    memset(bp->data, 0, BLOCK_SIZE);
    log_write(bp);
    bio_release(bp);
}

// Blocks.

/// Allocate a zeroed disk block.
/// returns 0 if out of disk space.
static uint32_t balloc(uint32_t dev)
{
    int b, bi, m;
    struct buf *bp;

    bp = 0;
    for (b = 0; b < sb.size; b += BPB)
    {
        bp = bio_read(dev, BBLOCK(b, sb));
        for (bi = 0; bi < BPB && b + bi < sb.size; bi++)
        {
            m = 1 << (bi % 8);
            if ((bp->data[bi / 8] & m) == 0)
            {                           // Is block free?
                bp->data[bi / 8] |= m;  // Mark block in use.
                log_write(bp);
                bio_release(bp);
                block_zero(dev, b + bi);
                return b + bi;
            }
        }
        bio_release(bp);
    }
    printk("balloc: out of blocks\n");
    return 0;
}

/// Free a disk block.
static void bfree(int dev, uint32_t b)
{
    struct buf *bp;
    int bi, m;

    bp = bio_read(dev, BBLOCK(b, sb));
    bi = b % BPB;
    m = 1 << (bi % 8);
    if ((bp->data[bi / 8] & m) == 0) panic("freeing free block");
    bp->data[bi / 8] &= ~m;
    log_write(bp);
    bio_release(bp);
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

struct
{
    struct spinlock lock;
    struct inode inode[NINODE];
} itable;

void inode_init()
{
    int i = 0;

    spin_lock_init(&itable.lock, "itable");
    for (i = 0; i < NINODE; i++)
    {
        sleep_lock_init(&itable.inode[i].lock, "inode");
    }
}

static struct inode *iget(uint32_t dev, uint32_t inum);

/// Allocate an inode on device dev.
/// Mark it as allocated by  giving it type type.
/// Returns an unlocked but allocated and referenced inode,
/// or NULL if there is no free inode.
struct inode *inode_alloc(uint32_t dev, short type)
{
    int inum;
    struct buf *bp;
    struct vx6fs_dinode *dip;

    for (inum = 1; inum < sb.ninodes; inum++)
    {
        bp = bio_read(dev, IBLOCK(inum, sb));
        dip = (struct vx6fs_dinode *)bp->data + inum % IPB;
        if (dip->type == 0)
        {  // a free inode
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            log_write(bp);  // mark it allocated on the disk
            bio_release(bp);
            return iget(dev, inum);
        }
        bio_release(bp);
    }
    printk("inode_alloc: no inodes\n");
    return 0;
}

/// Copy a modified in-memory inode to disk.
/// Must be called after every change to an ip->xxx field
/// that lives on disk.
/// Caller must hold ip->lock.
void inode_update(struct inode *ip)
{
    struct buf *bp;
    struct vx6fs_dinode *dip;

    bp = bio_read(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct vx6fs_dinode *)bp->data + ip->inum % IPB;
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ip->nlink;
    dip->size = ip->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    log_write(bp);
    bio_release(bp);
}

/// Find the inode with number inum on device dev
/// and return the in-memory copy. Does not lock
/// the inode and does not read it from disk.
static struct inode *iget(uint32_t dev, uint32_t inum)
{
    struct inode *ip, *empty;

    spin_lock(&itable.lock);

    // Is the inode already in the table?
    empty = 0;
    for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++)
    {
        if (ip->ref > 0 && ip->dev == dev && ip->inum == inum)
        {
            ip->ref++;
            spin_unlock(&itable.lock);
            return ip;
        }
        if (empty == 0 && ip->ref == 0)  // Remember empty slot.
            empty = ip;
    }

    // Recycle an inode entry.
    if (empty == 0) panic("iget: no inodes");

    ip = empty;
    ip->dev = dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0;
    spin_unlock(&itable.lock);

    return ip;
}

/// Increment reference count for ip.
/// Returns ip to enable ip = inode_dup(ip1) idiom.
struct inode *inode_dup(struct inode *ip)
{
    spin_lock(&itable.lock);
    ip->ref++;
    spin_unlock(&itable.lock);
    return ip;
}

/// Lock the given inode.
/// Reads the inode from disk if necessary.
void inode_lock(struct inode *ip)
{
    struct buf *bp;
    struct vx6fs_dinode *dip;

    if (ip == 0 || ip->ref < 1) panic("inode_lock");

    sleep_lock(&ip->lock);

    if (ip->valid == 0)
    {
        bp = bio_read(ip->dev, IBLOCK(ip->inum, sb));
        dip = (struct vx6fs_dinode *)bp->data + ip->inum % IPB;
        ip->type = dip->type;
        ip->major = dip->major;
        ip->minor = dip->minor;
        ip->nlink = dip->nlink;
        ip->size = dip->size;
        memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
        bio_release(bp);
        ip->valid = 1;
        if (ip->type == 0) panic("inode_lock: no type");
    }
}

/// Unlock the given inode.
void inode_unlock(struct inode *ip)
{
    if (ip == 0 || !sleep_lock_is_held_by_this_cpu(&ip->lock) || ip->ref < 1)
        panic("inode_unlock");

    sleep_unlock(&ip->lock);
}

/// Drop a reference to an in-memory inode.
/// If that was the last reference, the inode table entry can
/// be recycled.
/// If that was the last reference and the inode has no links
/// to it, free the inode (and its content) on disk.
/// All calls to inode_put() must be inside a transaction in
/// case it has to free the inode.
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
        ip->type = 0;
        inode_update(ip);
        ip->valid = 0;

        sleep_unlock(&ip->lock);

        spin_lock(&itable.lock);
    }

    ip->ref--;
    spin_unlock(&itable.lock);
}

/// Common idiom: unlock, then put.
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

/// Return the disk block address of the nth block in inode ip.
/// If there is no such block, bmap allocates one.
/// returns 0 if out of disk space.
static uint32_t bmap(struct inode *ip, uint32_t bn)
{
    uint32_t addr, *a;
    struct buf *bp;

    if (bn < NDIRECT)
    {
        if ((addr = ip->addrs[bn]) == 0)
        {
            addr = balloc(ip->dev);
            if (addr == 0) return 0;
            ip->addrs[bn] = addr;
        }
        return addr;
    }
    bn -= NDIRECT;

    if (bn < NINDIRECT)
    {
        // Load indirect block, allocating if necessary.
        if ((addr = ip->addrs[NDIRECT]) == 0)
        {
            addr = balloc(ip->dev);
            if (addr == 0) return 0;
            ip->addrs[NDIRECT] = addr;
        }
        bp = bio_read(ip->dev, addr);
        a = (uint32_t *)bp->data;
        if ((addr = a[bn]) == 0)
        {
            addr = balloc(ip->dev);
            if (addr)
            {
                a[bn] = addr;
                log_write(bp);
            }
        }
        bio_release(bp);
        return addr;
    }

    panic("bmap: out of range");
}

/// Truncate inode (discard contents).
/// Caller must hold ip->lock.
void inode_trunc(struct inode *ip)
{
    int i, j;
    struct buf *bp;
    uint32_t *a;

    for (i = 0; i < NDIRECT; i++)
    {
        if (ip->addrs[i])
        {
            bfree(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if (ip->addrs[NDIRECT])
    {
        bp = bio_read(ip->dev, ip->addrs[NDIRECT]);
        a = (uint32_t *)bp->data;
        for (j = 0; j < NINDIRECT; j++)
        {
            if (a[j]) bfree(ip->dev, a[j]);
        }
        bio_release(bp);
        bfree(ip->dev, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    ip->size = 0;
    inode_update(ip);
}

/// Copy stat information from inode.
/// Caller must hold ip->lock.
void inode_stat(struct inode *ip, struct stat *st)
{
    st->st_dev = ip->dev;
    st->st_ino = ip->inum;
    st->type = ip->type;
    st->st_nlink = ip->nlink;
    st->st_size = ip->size;
}

/// Read data from inode.
/// Caller must hold ip->lock.
/// If addr_is_userspace==1, then dst is a user virtual address;
/// otherwise, dst is a kernel address.
int inode_read(struct inode *ip, int addr_is_userspace, uint64_t dst,
               uint32_t off, uint32_t n)
{
    uint32_t tot, m;
    struct buf *bp;

    if (off > ip->size || off + n < off) return 0;
    if (off + n > ip->size) n = ip->size - off;

    for (tot = 0; tot < n; tot += m, off += m, dst += m)
    {
        uint32_t addr = bmap(ip, off / BLOCK_SIZE);
        if (addr == 0) break;
        bp = bio_read(ip->dev, addr);
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

/// Write data to inode.
/// Caller must hold ip->lock.
/// If addr_is_userspace==1, then src is a user virtual address;
/// otherwise, src is a kernel address.
/// Returns the number of bytes successfully written.
/// If the return value is less than the requested n,
/// there was an error of some kind.
int inode_write(struct inode *ip, int addr_is_userspace, uint64_t src,
                uint32_t off, uint32_t n)
{
    uint32_t tot, m;
    struct buf *bp;

    if (off > ip->size || off + n < off) return -1;
    if (off + n > MAXFILE * BLOCK_SIZE) return -1;

    for (tot = 0; tot < n; tot += m, off += m, src += m)
    {
        uint32_t addr = bmap(ip, off / BLOCK_SIZE);
        if (addr == 0) break;
        bp = bio_read(ip->dev, addr);
        m = min(n - tot, BLOCK_SIZE - off % BLOCK_SIZE);
        if (either_copyin(bp->data + (off % BLOCK_SIZE), addr_is_userspace, src,
                          m) == -1)
        {
            bio_release(bp);
            break;
        }
        log_write(bp);
        bio_release(bp);
    }

    if (off > ip->size) ip->size = off;

    // write the i-node back to disk even if the size didn't change
    // because the loop above might have called bmap() and added a new
    // block to ip->addrs[].
    inode_update(ip);

    return tot;
}

// Directories

int file_name_cmp(const char *s, const char *t)
{
    return strncmp(s, t, XV6_NAME_MAX);
}

/// Look for a directory entry in a directory.
/// If found, set *poff to byte offset of entry.
struct inode *inode_dir_lookup(struct inode *dir, char *name, uint32_t *poff)
{
    uint32_t off, inum;
    struct xv6fs_dirent de;

    if (dir->type != XV6_FT_DIR) panic("inode_dir_lookup not DIR");

    for (off = 0; off < dir->size; off += sizeof(de))
    {
        if (inode_read(dir, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
            panic("inode_dir_lookup read");
        if (de.inum == 0) continue;
        if (file_name_cmp(name, de.name) == 0)
        {
            // entry matches path element
            if (poff) *poff = off;
            inum = de.inum;
            return iget(dir->dev, inum);
        }
    }

    return 0;
}

/// Write a new directory entry (name, inum) into the directory dir.
/// Returns 0 on success, -1 on failure (e.g. out of disk blocks).
int inode_dir_link(struct inode *dir, char *name, uint32_t inum)
{
    int off;
    struct xv6fs_dirent de;
    struct inode *ip;

    // Check that name is not present.
    if ((ip = inode_dir_lookup(dir, name, 0)) != 0)
    {
        inode_put(ip);
        return -1;
    }

    // Look for an empty xv6fs_dirent.
    for (off = 0; off < dir->size; off += sizeof(de))
    {
        if (inode_read(dir, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
            panic("inode_dir_link read");
        if (de.inum == 0) break;
    }

    strncpy(de.name, name, XV6_NAME_MAX);
    de.inum = inum;
    if (inode_write(dir, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
        return -1;

    return 0;
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
static char *skipelem(char *path, char *name)
{
    char *s;
    int len;

    while (*path == '/') path++;
    if (*path == 0) return 0;
    s = path;
    while (*path != '/' && *path != 0) path++;
    len = path - s;
    if (len >= XV6_NAME_MAX)
        memmove(name, s, XV6_NAME_MAX);
    else
    {
        memmove(name, s, len);
        name[len] = 0;
    }
    while (*path == '/') path++;
    return path;
}

/// Look up and return the inode for a path name.
/// If get_parent != 0, return the inode for the parent and copy the final
/// path element into name, which must have room for XV6_NAME_MAX bytes.
/// Must be called inside a transaction since it calls inode_put().
static struct inode *namex(char *path, int get_parent, char *name)
{
    struct inode *ip, *next;

    if (*path == '/')
        ip = iget(ROOT_DEVICE_NUMBER, ROOT_INODE);
    else
        ip = inode_dup(get_current()->cwd);

    while ((path = skipelem(path, name)) != 0)
    {
        inode_lock(ip);
        if (ip->type != XV6_FT_DIR)
        {
            inode_unlock_put(ip);
            return 0;
        }
        if (get_parent && *path == '\0')
        {
            // Stop one level early.
            inode_unlock(ip);
            return ip;
        }
        if ((next = inode_dir_lookup(ip, name, 0)) == 0)
        {
            inode_unlock_put(ip);
            return 0;
        }
        inode_unlock_put(ip);
        ip = next;
    }
    if (get_parent)
    {
        inode_put(ip);
        return 0;
    }
    return ip;
}

struct inode *inode_from_path(char *path)
{
    char name[XV6_NAME_MAX];
    return namex(path, 0, name);
}

struct inode *inode_of_parent_from_path(char *path, char *name)
{
    return namex(path, 1, name);
}
