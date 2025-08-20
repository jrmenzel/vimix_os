/* SPDX-License-Identifier: MIT */

#include <fs/vfs.h>
#include <fs/xv6fs/log.h>
#include <fs/xv6fs/xv6fs.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
#include <kernel/container_of.h>
#include <kernel/errno.h>
#include <kernel/fcntl.h>
#include <kernel/file.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/string.h>
#include <kernel/xv6fs.h>
#include <lib/minmax.h>

/// @brief Truncate inode (discard contents), does not call
/// xv6fs_sops_write_inode() and does not start a FS log!
/// @param ip inode to truncate.
void xv6fs_trunc(struct inode *ip);

/// one g_xv6fs_sb_private per mount
struct xv6fs_sb_private g_xv6fs_sb_private[MAX_MOUNTED_FILE_SYSTEMS] = {0};

struct xv6fs_sb_private *get_free_sb_private()
{
    for (size_t i = 0; i < MAX_MOUNTED_FILE_SYSTEMS; ++i)
    {
        if (g_xv6fs_sb_private[i].sb.magic == 0)
        {
            return &g_xv6fs_sb_private[i];
        }
    }
    return NULL;
}

struct file_system_type xv6_file_system_type;

extern struct super_block g_active_file_systems[MAX_MOUNTED_FILE_SYSTEMS];

struct xv6fs_inode
{
    struct inode ino;

    /// Inode content
    ///
    /// The content (data) associated with each inode is stored
    /// in blocks on the disk. The first XV6FS_N_DIRECT_BLOCKS block numbers
    /// are listed in ip->addrs[].  The next XV6FS_N_INDIRECT_BLOCKS blocks are
    /// listed in block ip->addrs[XV6FS_N_DIRECT_BLOCKS].
    uint32_t addrs[XV6FS_N_DIRECT_BLOCKS + 1];
};

#define xv6fs_inode_from_inode(ptr) container_of(ptr, struct xv6fs_inode, ino)

/// In memory inodes, call xv6fs_init() before use.
struct
{
    /// The xv6fs_itable.lock spin-lock protects the allocation of
    /// xv6fs_itable entries. Since ip->ref indicates whether an entry
    /// is free, and ip->dev and ip->inum indicate which i-node
    /// an entry holds, one must hold xv6fs_itable.lock while using any
    /// of those fields.
    struct spinlock lock;
    struct xv6fs_inode inode[XV6FS_MAX_ACTIVE_INODES];
} xv6fs_itable;

const char *XV6_FS_NAME = "xv6fs";

// super block operations
struct super_operations xv6fs_s_op = {
    iget_root : xv6fs_sops_iget_root,
    alloc_inode : xv6fs_sops_alloc_inode,
    write_inode : xv6fs_sops_write_inode
};

// inode operations
struct inode_operations xv6fs_i_op = {
    iops_create : xv6fs_iops_create,
    iops_open : xv6fs_iops_open,
    iops_read_in : xv6fs_iops_read_in,
    iops_dup : xv6fs_iops_dup,
    iops_put : xv6fs_iops_put,
    iops_dir_lookup : xv6fs_iops_dir_lookup,
    iops_dir_link : xv6fs_iops_dir_link,
    iops_get_dirent : xv6fs_iops_get_dirent,
    iops_read : xv6fs_iops_read,
    iops_link : xv6fs_iops_link,
    iops_unlink : xv6fs_iops_unlink
};

struct file_operations xv6fs_f_op = {fops_write : xv6fs_fops_write};

xv6fs_file_type imode_to_xv6_file_type(mode_t imode);

mode_t xv6_file_type_to_imode(xv6fs_file_type type);

ssize_t xv6fs_init_fs_super_block(struct super_block *sb_in, const void *data);
void xv6fs_kill_sb(struct super_block *sb_in);

/// Allocate a zeroed disk block.
/// returns 0 if out of disk space.
uint32_t balloc(struct super_block *sb);

/// Free a disk block.
void bfree(struct super_block *sb, uint32_t b);

void xv6fs_init()
{
    xv6_file_system_type.name = XV6_FS_NAME;

    xv6_file_system_type.next = NULL;
    xv6_file_system_type.init_fs_super_block = xv6fs_init_fs_super_block;
    xv6_file_system_type.kill_sb = xv6fs_kill_sb;

    spin_lock_init(&xv6fs_itable.lock, "xv6fs_itable");
    for (size_t i = 0; i < XV6FS_MAX_ACTIVE_INODES; i++)
    {
        sleep_lock_init(&xv6fs_itable.inode[i].ino.lock, "inode");
        xv6fs_itable.inode[i].ino.inum = XV6FS_UNUSED_INODE;
    }

    register_file_system(&xv6_file_system_type);
}

xv6fs_file_type imode_to_xv6_file_type(mode_t imode)
{
    if (S_ISREG(imode)) return XV6_FT_FILE;
    if (S_ISDIR(imode)) return XV6_FT_DIR;
    if (S_ISCHR(imode)) return XV6_FT_CHAR_DEVICE;
    if (S_ISBLK(imode)) return XV6_FT_BLOCK_DEVICE;

    return XV6_FT_UNUSED;
}

mode_t xv6_file_type_to_imode(xv6fs_file_type type)
{
    if (type == XV6_FT_FILE) return S_IFREG | DEFAULT_ACCESS_MODES;
    if (type == XV6_FT_DIR) return S_IFDIR | DEFAULT_ACCESS_MODES;
    if (type == XV6_FT_CHAR_DEVICE) return S_IFCHR | DEFAULT_ACCESS_MODES;
    if (type == XV6_FT_BLOCK_DEVICE) return S_IFBLK | DEFAULT_ACCESS_MODES;

    return 0;
}

ssize_t xv6fs_init_fs_super_block(struct super_block *sb_in, const void *data)
{
    // data is used for file system specific mount parameters
    // ignore those here

    struct xv6fs_sb_private *priv = get_free_sb_private();
    if (priv == NULL)
    {
        return -ENOMEM;
    }
    sb_in->s_fs_info = (void *)priv;

    dev_t dev = sb_in->dev;
    struct buf *first_block = bio_read(dev, XV6FS_SUPER_BLOCK_NUMBER);

    struct xv6fs_superblock *vx6_sb =
        (struct xv6fs_superblock *)first_block->data;
    if (vx6_sb->magic != XV6FS_MAGIC)
    {
        // wrong file system
        printk("xv6fs error: wrong file system\n");
        bio_release(first_block);
        return -EINVAL;
    }

    memmove(&(priv->sb), vx6_sb, sizeof(struct xv6fs_superblock));
    log_init(&(priv->log), dev, &(priv->sb));
    bio_release(first_block);

    sb_in->s_type = &xv6_file_system_type;
    sb_in->s_op = &xv6fs_s_op;
    sb_in->i_op = &xv6fs_i_op;
    sb_in->f_op = &xv6fs_f_op;

    return 0;
}

void xv6fs_kill_sb(struct super_block *sb_in)
{
    struct xv6fs_sb_private *priv = (struct xv6fs_sb_private *)sb_in->s_fs_info;
    DEBUG_EXTRA_ASSERT(priv != NULL, "private data should be set since mount");
    priv->sb.magic = 0;  // flags private block as unused
    sb_in->s_fs_info = NULL;
}

struct inode *xv6fs_iops_lookup(struct inode *iparent, char name[NAME_MAX],
                                mode_t mode, int32_t flags)
{
    log_begin_fs_transaction(iparent->i_sb);
    inode_lock(iparent);
    struct inode *ip = xv6fs_iops_dir_lookup(iparent, name, NULL);
    if (ip == NULL)
    {
        log_end_fs_transaction(iparent->i_sb);
        return NULL;
    }

    inode_unlock_put(iparent);
    inode_lock(ip);
    if (S_ISREG(mode) &&
        (S_ISREG(ip->i_mode) || S_ISCHR(ip->i_mode) || S_ISBLK(ip->i_mode)))
    {
        if (flags & O_TRUNC)
        {
            // truncate if needed
            xv6fs_trunc(ip);
            xv6fs_sops_write_inode(ip);
        }
#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
        strncpy(ip->path, name, PATH_MAX);
#endif
        log_end_fs_transaction(iparent->i_sb);
        return ip;
    }
    inode_unlock_put(ip);
    log_end_fs_transaction(iparent->i_sb);
    return NULL;
}

struct inode *xv6fs_iops_create_internal(struct inode *iparent,
                                         char name[NAME_MAX], mode_t mode,
                                         int32_t flags, dev_t device)
{
    // if the inode already exists, return it
    inode_lock(iparent);
    struct inode *ip = xv6fs_iops_dir_lookup(iparent, name, NULL);
    if (ip != NULL)
    {
        inode_unlock(iparent);
        inode_lock(ip);
        if (S_ISREG(mode) &&
            (S_ISREG(ip->i_mode) || S_ISCHR(ip->i_mode) || S_ISBLK(ip->i_mode)))
        {
            if (flags & O_TRUNC)
            {
                // truncate if needed
                xv6fs_trunc(ip);
                xv6fs_sops_write_inode(ip);
            }
#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
            strncpy(ip->path, name, PATH_MAX);
#endif
            return ip;
        }
        inode_unlock_put(ip);
        return NULL;
    }

    // create new inode
    ip = xv6fs_sops_alloc_inode(iparent->i_sb, mode);
    if (ip == NULL)
    {
        inode_unlock(iparent);
        return NULL;
    }

    inode_lock(ip);
    if (device != INVALID_DEVICE)
    {
        // device node
        ip->dev = device;
    }
    else
    {
        // regular file
        ip->dev = ip->i_sb->dev;
    }
    ip->nlink = 1;
    xv6fs_sops_write_inode(ip);

#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
    strncpy(ip->path, name, PATH_MAX);
#endif

    if (S_ISDIR(mode))
    {
        // Create . and .. entries.
        // No ip->nlink++ for ".": avoid cyclic ref count.
        if (inode_dir_link(ip, ".", ip->inum) < 0 ||
            inode_dir_link(ip, "..", iparent->inum) < 0)
        {
            goto fail;
        }
    }

    if (inode_dir_link(iparent, name, ip->inum) < 0)
    {
        goto fail;
    }

    if (S_ISDIR(mode))
    {
        // now that success is guaranteed:
        iparent->nlink++;  // for ".."
        xv6fs_sops_write_inode(iparent);
    }

    inode_unlock(iparent);

    return ip;

fail:
    // something went wrong. de-allocate ip.
    ip->nlink = 0;
    xv6fs_sops_write_inode(ip);
    inode_unlock_put(ip);
    inode_unlock(iparent);
    return NULL;
}

struct inode *xv6fs_iops_open(struct inode *iparent, char name[NAME_MAX],
                              int32_t flags)
{
    inode_lock(iparent);
    struct inode *ip = xv6fs_iops_dir_lookup(iparent, name, NULL);
    inode_unlock(iparent);
    if (ip == NULL)
    {
        // file not found
        return NULL;
    }

    inode_lock(ip);

    if (S_ISREG(ip->i_mode) && flags & O_TRUNC)
    {
        // truncate if needed
        log_begin_fs_transaction(iparent->i_sb);
        xv6fs_trunc(ip);
        xv6fs_sops_write_inode(ip);
        log_end_fs_transaction(iparent->i_sb);
    }
#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
    strncpy(ip->path, name, PATH_MAX);
#endif
    return ip;  // return locked
}

struct inode *xv6fs_iops_create(struct inode *iparent, char name[NAME_MAX],
                                mode_t mode, int32_t flags, dev_t device)
{
    log_begin_fs_transaction(iparent->i_sb);
    struct inode *ip =
        xv6fs_iops_create_internal(iparent, name, mode, flags, device);
    log_end_fs_transaction(iparent->i_sb);
    return ip;
}

struct inode *xv6fs_sops_alloc_inode(struct super_block *sb, mode_t mode)
{
    struct xv6fs_sb_private *priv = (struct xv6fs_sb_private *)sb->s_fs_info;
    struct xv6fs_superblock *xsb = &(priv->sb);

    for (size_t inum = 1; inum < xsb->ninodes; inum++)
    {
        struct buf *bp = bio_read(sb->dev, XV6FS_BLOCK_OF_INODE_P(inum, xsb));
        struct xv6fs_dinode *dip =
            (struct xv6fs_dinode *)bp->data + inum % XV6FS_INODES_PER_BLOCK;

        if (dip->type == XV6_FT_UNUSED)
        {
            // a free inode
            memset(dip, 0, sizeof(*dip));
            dip->type = imode_to_xv6_file_type(mode);
            dip->major = 0;
            dip->minor = 0;
            log_write(&(priv->log), bp);  // mark it allocated on the disk
            bio_release(bp);
            return xv6fs_iget(sb, inum);
        }
        bio_release(bp);
    }

    printk("xv6fs_sops_alloc_inode: no inodes\n");
    return NULL;
}

int xv6fs_sops_write_inode(struct inode *ip)
{
    struct xv6fs_sb_private *priv =
        (struct xv6fs_sb_private *)ip->i_sb->s_fs_info;
    struct xv6fs_superblock *xsb = &(priv->sb);

    size_t block_of_inode = XV6FS_BLOCK_OF_INODE_P(ip->inum, xsb);

    struct buf *bp = bio_read(ip->i_sb->dev, block_of_inode);
    struct xv6fs_dinode *dip =
        (struct xv6fs_dinode *)bp->data + ip->inum % XV6FS_INODES_PER_BLOCK;
    dip->type = imode_to_xv6_file_type(ip->i_mode);

    if (ip->dev == ip->i_sb->dev)
    {
        // map whatever device the filesystem is on to 0
        dip->major = 0;
        dip->minor = 0;
    }
    else
    {
        dip->major = MAJOR(ip->dev);
        dip->minor = MINOR(ip->dev);
    }

    dip->nlink = ip->nlink;
    dip->size = ip->size;
    struct xv6fs_inode *xv_ip = xv6fs_inode_from_inode(ip);
    memmove(dip->addrs, xv_ip->addrs, sizeof(xv_ip->addrs));
    log_write(&(priv->log), bp);
    bio_release(bp);

    return 0;
}

void xv6fs_iops_read_in(struct inode *ip)
{
    struct xv6fs_superblock *xsb =
        &((struct xv6fs_sb_private *)ip->i_sb->s_fs_info)->sb;

    size_t block_of_inode = XV6FS_BLOCK_OF_INODE_P(ip->inum, xsb);

    struct buf *bp = bio_read(ip->i_sb->dev, block_of_inode);
    struct xv6fs_dinode *dip =
        (struct xv6fs_dinode *)bp->data + ip->inum % XV6FS_INODES_PER_BLOCK;
    ip->i_mode = xv6_file_type_to_imode(dip->type);

    if (dip->major == 0 && dip->minor == 0)
    {
        ip->dev = ip->i_sb->dev;  // unmap device 0 to whatever device the
                                  // filesystem is on
    }
    else
    {
        ip->dev = MKDEV(dip->major, dip->minor);
    }

    ip->nlink = dip->nlink;
    ip->size = dip->size;
    struct xv6fs_inode *xv_ip = xv6fs_inode_from_inode(ip);
    memmove(xv_ip->addrs, dip->addrs, sizeof(xv_ip->addrs));
    bio_release(bp);
}

void xv6fs_trunc(struct inode *ip)
{
    struct xv6fs_inode *xv_ip = xv6fs_inode_from_inode(ip);
    for (size_t i = 0; i < XV6FS_N_DIRECT_BLOCKS; i++)
    {
        if (xv_ip->addrs[i])
        {
            bfree(ip->i_sb, xv_ip->addrs[i]);
            xv_ip->addrs[i] = 0;
        }
    }

    if (xv_ip->addrs[XV6FS_N_DIRECT_BLOCKS])
    {
        struct buf *bp = bio_read(ip->dev, xv_ip->addrs[XV6FS_N_DIRECT_BLOCKS]);
        uint32_t *a = (uint32_t *)bp->data;
        for (size_t j = 0; j < XV6FS_N_INDIRECT_BLOCKS; j++)
        {
            if (a[j])
            {
                bfree(ip->i_sb, a[j]);
            }
        }
        bio_release(bp);
        bfree(ip->i_sb, xv_ip->addrs[XV6FS_N_DIRECT_BLOCKS]);
        xv_ip->addrs[XV6FS_N_DIRECT_BLOCKS] = 0;
    }

    ip->size = 0;
}

/// Zero a block.
static void block_zero(dev_t dev, struct log *log, uint32_t blockno)
{
    struct buf *bp = bio_read(dev, blockno);
    memset(bp->data, 0, BLOCK_SIZE);
    log_write(log, bp);
    bio_release(bp);
}

uint32_t balloc(struct super_block *sb)
{
    struct xv6fs_sb_private *priv = (struct xv6fs_sb_private *)sb->s_fs_info;
    struct xv6fs_superblock *xsb = &(priv->sb);

    for (uint32_t b = 0; b < xsb->size; b += XV6FS_BMAP_BITS_PER_BLOCK)
    {
        struct buf *bp =
            bio_read(sb->dev, XV6FS_BMAP_BLOCK_OF_BIT(b, xsb->bmapstart));
        for (uint32_t bi = 0;
             bi < XV6FS_BMAP_BITS_PER_BLOCK && b + bi < xsb->size; bi++)
        {
            uint32_t m = 1 << (bi % 8);
            if ((bp->data[bi / 8] & m) == 0)
            {                           // Is block free?
                bp->data[bi / 8] |= m;  // Mark block in use.
                log_write(&(priv->log), bp);
                bio_release(bp);
                block_zero(sb->dev, &(priv->log), b + bi);
                return b + bi;
            }
        }
        bio_release(bp);
    }
    printk("balloc: out of blocks\n");
    return 0;
}

/// Free a disk block.
void bfree(struct super_block *sb, uint32_t b)
{
    struct xv6fs_sb_private *priv = (struct xv6fs_sb_private *)sb->s_fs_info;
    struct xv6fs_superblock *xsb = &(priv->sb);

    struct buf *bp =
        bio_read(sb->dev, XV6FS_BMAP_BLOCK_OF_BIT(b, xsb->bmapstart));
    int32_t bi = b % XV6FS_BMAP_BITS_PER_BLOCK;
    int32_t m = 1 << (bi % 8);

    if ((bp->data[bi / 8] & m) == 0)
    {
        panic("freeing free block");
    }
    bp->data[bi / 8] &= ~m;
    log_write(&(priv->log), bp);
    bio_release(bp);
}

/// Return the disk block address of the nth block in inode ip.
/// If there is no such block, bmap allocates one.
/// returns 0 if out of disk space.
size_t bmap(struct inode *ip, uint32_t bn)
{
    struct xv6fs_inode *xv_ip = xv6fs_inode_from_inode(ip);
    if (bn < XV6FS_N_DIRECT_BLOCKS)
    {
        size_t addr = xv_ip->addrs[bn];
        if (addr == 0)
        {
            addr = balloc(ip->i_sb);
            if (addr == 0)
            {
                return 0;
            }
            xv_ip->addrs[bn] = addr;
        }
        return addr;
    }
    bn -= XV6FS_N_DIRECT_BLOCKS;

    if (bn < XV6FS_N_INDIRECT_BLOCKS)
    {
        // Load indirect block, allocating if necessary.
        size_t addr = xv_ip->addrs[XV6FS_N_DIRECT_BLOCKS];
        if (addr == 0)
        {
            addr = balloc(ip->i_sb);
            if (addr == 0)
            {
                return 0;
            }
            xv_ip->addrs[XV6FS_N_DIRECT_BLOCKS] = addr;
        }
        struct buf *bp = bio_read(ip->dev, addr);
        uint32_t *a = (uint32_t *)bp->data;
        addr = a[bn];
        if (addr == 0)
        {
            addr = balloc(ip->i_sb);
            if (addr)
            {
                a[bn] = addr;
                struct xv6fs_sb_private *priv =
                    (struct xv6fs_sb_private *)ip->i_sb->s_fs_info;
                log_write(&(priv->log), bp);
            }
        }
        bio_release(bp);
        return addr;
    }

    panic("bmap: out of range");
    return 0;
}

struct inode *xv6fs_iget(struct super_block *sb, uint32_t inum)
{
    if (sb == NULL) return NULL;

    spin_lock(&xv6fs_itable.lock);

    // Is the inode already in the table?
    struct inode *empty = NULL;
    struct xv6fs_inode *xv_ip = NULL;
    for (xv_ip = &xv6fs_itable.inode[0];
         xv_ip < &xv6fs_itable.inode[XV6FS_MAX_ACTIVE_INODES]; xv_ip++)
    {
        struct inode *ip = &(xv_ip->ino);
        DEBUG_EXTRA_ASSERT(ip != NULL, "invalid inode");

        if (ip->ref > 0 && ip->i_sb->dev == sb->dev && ip->inum == inum)
        {
            ip->ref++;
            spin_unlock(&xv6fs_itable.lock);
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
        panic("xv6fs_iget: no inodes left. See XV6FS_MAX_ACTIVE_INODES.");
    }

    struct inode *ip = empty;
    ip->i_sb = sb;
    ip->dev = sb->dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0;
    ip->is_mounted_on = NULL;
    spin_unlock(&xv6fs_itable.lock);

    return ip;
}

struct inode *xv6fs_iops_dup(struct inode *ip)
{
    spin_lock(&xv6fs_itable.lock);
    ip->ref++;
    spin_unlock(&xv6fs_itable.lock);
    return ip;
}

void xv6fs_iops_put(struct inode *ip)
{
    spin_lock(&xv6fs_itable.lock);

    if (ip->ref == 1 && ip->valid && ip->nlink == 0)
    {
        struct process *proc = get_current();
        bool external_fs_transaction = (proc->debug_log_depth != 0);
        // inode has no links and no other references: truncate and free.

        if (!external_fs_transaction)
        {
            // rare case: e.g. a file was deleted while someone still had a
            // reference (namex() during traversal?). Now the inode_put() of
            // the second process will trigger the delete on the FS. This is
            // fine and might happen inside of the FS transaction of another
            // FS syscall, but if it doesn't, a new FS transaction must be
            // started. Otherwise this might also trigger an error if no
            // other FS transaction is active...

            // to avoid deadlocks, release itable lock:
            spin_unlock(&xv6fs_itable.lock);

            log_begin_fs_transaction(ip->i_sb);

            // regain lock:
            spin_lock(&xv6fs_itable.lock);

            // no check is needed that the inode is still to be deleted, as
            // this thread had the last reference and it is not accessible
            // via the FS anymore.
            DEBUG_EXTRA_ASSERT(
                ip->ref == 1 && ip->valid && ip->nlink == 0,
                "No-one should have been able to change this inode!");
        }

        // ip->ref == 1 means no other process can have ip locked,
        // so this sleep_lock() won't block (or deadlock).
        sleep_lock(&ip->lock);

        spin_unlock(&xv6fs_itable.lock);

        xv6fs_trunc(ip);
        ip->i_mode = 0;
        xv6fs_sops_write_inode(ip);
        ip->valid = 0;

        sleep_unlock(&ip->lock);

        if (!external_fs_transaction)
        {
            log_end_fs_transaction(ip->i_sb);
        }

        spin_lock(&xv6fs_itable.lock);
    }

    DEBUG_EXTRA_ASSERT(ip->ref > 0,
                       "Can't put an inode that is not held by anyone");
    ip->ref--;
    spin_unlock(&xv6fs_itable.lock);
}

bool inode_is_mounted_fs_root(struct inode *dir)
{
    return ((dir == dir->i_sb->s_root) && (dir->i_sb->imounted_on));
}

struct inode *xv6fs_iops_dir_lookup(struct inode *dir, const char *name,
                                    uint32_t *poff)
{
    struct xv6fs_dirent de;
    for (size_t off = 0; off < dir->size; off += sizeof(de))
    {
        if (inode_read(dir, false, (size_t)&de, off, sizeof(de)) != sizeof(de))
        {
            panic("xv6fs_iops_dir_lookup read error");
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
            if (inode_is_mounted_fs_root(dir) &&
                (file_name_cmp("..", de.name) == 0))
            {
                inode_lock(dir->i_sb->imounted_on);
                struct inode *ret =
                    VFS_INODE_DIR_LOOKUP(dir->i_sb->imounted_on, "..", poff);
                inode_unlock(dir->i_sb->imounted_on);
                return ret;
            }
            return xv6fs_iget(dir->i_sb, (uint32_t)de.inum);
        }
    }

    return NULL;
}

int xv6fs_iops_dir_link(struct inode *dir, char *name, uint32_t inum)
{
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

    ssize_t written = xv6fs_write(dir, false, (size_t)&de, off, sizeof(de));
    if (written != sizeof(de))
    {
        return -1;
    }

    return 0;
}

ssize_t xv6fs_iops_get_dirent(struct inode *dir, size_t dir_entry_addr,
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
            return read_bytes;  // 0 if no more dirents to read or -1 on
                                // error
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
    dir_entry.d_reclen = sizeof(struct dirent);
    strncpy(dir_entry.d_name, xv6_dir_entry.name, XV6_NAME_MAX);
    dir_entry.d_off = (long)(new_seek_pos);

    int32_t res = either_copyout(addr_is_userspace, dir_entry_addr,
                                 (void *)&dir_entry, sizeof(struct dirent));
    if (res < 0) return -EFAULT;

    return new_seek_pos;
}

ssize_t xv6fs_iops_read(struct inode *ip, bool addr_is_userspace, size_t dst,
                        size_t off, size_t n)
{
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

ssize_t xv6fs_write(struct inode *ip, bool src_addr_is_userspace, size_t src,
                    size_t off, size_t n)
{
    if (off > ip->size || off + n < off)
    {
        return -1;
    }

    if (off + n > XV6FS_MAX_FILE_SIZE_BLOCKS * BLOCK_SIZE)
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
        struct xv6fs_sb_private *priv =
            (struct xv6fs_sb_private *)ip->i_sb->s_fs_info;
        log_write(&(priv->log), bp);
        bio_release(bp);
    }

    if (off > ip->size)
    {
        ip->size = off;
    }

    // write the i-node back to disk even if the size didn't change
    // because the loop above might have called bmap() and added a new
    // block to ip->addrs[].
    xv6fs_sops_write_inode(ip);

    return tot;
}

ssize_t xv6fs_iops_link(struct inode *dir, struct inode *ip,
                        char name[NAME_MAX])
{
    log_begin_fs_transaction(ip->i_sb);
    ip->nlink++;
    xv6fs_sops_write_inode(ip);
    inode_unlock(ip);

    if (inode_dir_link(dir, name, ip->inum) < 0)
    {
        inode_unlock_put(dir);

        inode_lock(ip);
        ip->nlink--;
        xv6fs_sops_write_inode(ip);
        inode_unlock_put(ip);
        log_end_fs_transaction(ip->i_sb);
        return -EOTHER;
    }
    log_end_fs_transaction(ip->i_sb);

    inode_unlock_put(dir);
    inode_put(ip);

    return 0;
}

ssize_t xv6fs_fops_write(struct file *f, size_t addr, size_t n)
{
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    ssize_t max = ((MAX_OP_BLOCKS - 1 - 1 - 2) / 2) * BLOCK_SIZE;
    ssize_t i = 0;
    while (i < n)
    {
        ssize_t r;
        ssize_t n1 = n - i;
        if (n1 > max)
        {
            n1 = max;
        }

        log_begin_fs_transaction(f->ip->i_sb);
        inode_lock(f->ip);

        r = xv6fs_write(f->ip, true, addr + i, f->off, n1);
        if (r > 0)
        {
            f->off += r;
        }
        inode_unlock(f->ip);
        log_end_fs_transaction(f->ip->i_sb);

        if (r != n1)
        {
            return -1;
        }
        i += r;
    }
    return (i == n ? n : -1);
}

/// Is the directory dir empty except for "." and ".." ?
static int isdirempty(struct inode *dir)
{
    struct xv6fs_dirent de;

    for (size_t off = 2 * sizeof(de); off < dir->size; off += sizeof(de))
    {
        if (inode_read(dir, false, (size_t)&de, off, sizeof(de)) != sizeof(de))
        {
            panic("isdirempty: inode_read");
        }

        if (de.inum != 0)
        {
            return 0;
        }
    }
    return 1;
}

ssize_t xv6fs_iops_unlink(struct inode *dir, char name[NAME_MAX],
                          bool delete_files, bool delete_directories)
{
    log_begin_fs_transaction(dir->i_sb);
    inode_lock(dir);
    uint32_t off;
    struct inode *ip = xv6fs_iops_dir_lookup(dir, name, &off);
    if (ip == NULL)
    {
        inode_unlock_put(dir);
        log_end_fs_transaction(dir->i_sb);
        return -ENOENT;
    }
    inode_lock(ip);

    if (ip->nlink < 1)
    {
        panic("unlink: nlink < 1");
    }

    ssize_t error = 0;
    if (S_ISDIR(ip->i_mode) && (!delete_directories))
    {
        error = -EISDIR;
    }
    if (!S_ISDIR(ip->i_mode) && (!delete_files))
    {
        error = -ENOTDIR;
    }
    if (S_ISDIR(ip->i_mode) && !isdirempty(ip))
    {
        error = -ENOTEMPTY;
    }

    if (error != 0)
    {
        inode_unlock_put(ip);
        inode_unlock_put(dir);
        log_end_fs_transaction(dir->i_sb);
        return error;
    }

    // delete directory entry by over-writing it with zeros:
    struct xv6fs_dirent de;
    memset(&de, 0, sizeof(de));
    if (xv6fs_write(dir, false, (size_t)&de, off, sizeof(de)) != sizeof(de))
    {
        panic("xv6fs_iops_unlink: xv6fs_write");
    }

    if (S_ISDIR(ip->i_mode))
    {
        dir->nlink--;
        xv6fs_sops_write_inode(dir);
    }
    inode_unlock_put(dir);

    ip->nlink--;
    xv6fs_sops_write_inode(ip);
    inode_unlock_put(ip);

    log_end_fs_transaction(dir->i_sb);

    return 0;
}

void xv6fs_debug_print_inodes()
{
    printk("inodes:\n");
    for (size_t i = 0; i < XV6FS_MAX_ACTIVE_INODES; ++i)
    {
        struct inode *ip = &xv6fs_itable.inode[i].ino;
        if (ip->ref != 0)
        {
            debug_print_inode(ip);
            printk("\n");
        }
    }

    for (size_t i = 0; i < MAX_MINOR_DEVICES; ++i)
    {
        if (g_xv6fs_sb_private[i].sb.magic != 0)
        {
            struct log *l = &g_xv6fs_sb_private[i].log;
            printk("log %zd: 0x%zx | ", i, (size_t)l);
            printk("outstanding: %d, committing: %d", l->outstanding,
                   l->committing);
            printk(", size: %d, used: %d\n", l->size, l->lh.n);
        }
    }
}
