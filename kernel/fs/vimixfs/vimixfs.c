/* SPDX-License-Identifier: MIT */

#include <drivers/rtc.h>
#include <fs/dentry.h>
#include <fs/dentry_cache.h>
#include <fs/vfs.h>
#include <fs/vimixfs/bmap.h>
#include <fs/vimixfs/log.h>
#include <fs/vimixfs/vimixfs.h>
#include <fs/vimixfs/vimixfs_sysfs.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
#include <kernel/container_of.h>
#include <kernel/errno.h>
#include <kernel/fcntl.h>
#include <kernel/file.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/statvfs.h>
#include <kernel/string.h>
#include <kernel/vimixfs.h>
#include <lib/minmax.h>
#include <mm/kalloc.h>

/// @brief Truncate inode (discard contents), does not call
/// vimixfs_sops_write_inode() and does not start a FS log!
/// @param ip inode to truncate.
/// @param first_trunc_block First block to truncate, previous blocks are kept.
void vimixfs_trunc(struct inode *ip, size_t first_trunc_block);

struct file_system_type vimixfs_file_system_type;

const char *VIMIXFS_FS_NAME = "vimixfs";

// super block operations
struct super_operations vimixfs_s_op = {
    iget_root : vimixfs_sops_iget_root,
    alloc_inode : vimixfs_sops_alloc_inode,
    write_inode : vimixfs_sops_write_inode,
    statvfs : vimix_sops_statvfs
};

// inode operations
struct inode_operations vimixfs_i_op = {
    iops_create : vimixfs_iops_create,
    iops_mknod : vimixfs_iops_mknod,
    iops_mkdir : vimixfs_iops_mkdir,
    iops_put : vimixfs_iops_put,
    iops_lookup : vimixfs_iops_lookup,
    iops_get_dirent : vimixfs_iops_get_dirent,
    iops_read : vimixfs_iops_read,
    iops_link : vimixfs_iops_link,
    iops_unlink : vimixfs_iops_unlink,
    iops_rmdir : vimixfs_iops_rmdir,
    iops_truncate : vimixfs_iops_truncate,
    iops_chmod : vimixfs_iops_chmod,
    iops_chown : vimixfs_iops_chown
};

struct file_operations vimixfs_f_op = {
    fops_open : vimixfs_fops_open,
    fops_write : vimixfs_fops_write
};

syserr_t vimixfs_init_fs_super_block(struct super_block *sb_in,
                                     const void *data);
void vimixfs_kill_sb(struct super_block *sb_in);

/// Free a disk block.
void block_free(struct super_block *sb, uint32_t b);

void vimixfs_init()
{
    vimixfs_file_system_type.name = VIMIXFS_FS_NAME;

    vimixfs_file_system_type.next = NULL;
    vimixfs_file_system_type.init_fs_super_block = vimixfs_init_fs_super_block;
    vimixfs_file_system_type.kill_sb = vimixfs_kill_sb;

    register_file_system(&vimixfs_file_system_type);
}

syserr_t vimixfs_init_fs_super_block(struct super_block *sb_in,
                                     const void *data)
{
    // data is used for file system specific mount parameters
    // ignore those here
    dev_t dev = sb_in->dev;
    struct buf *first_block = bio_read(dev, VIMIXFS_SUPER_BLOCK_NUMBER);
    struct vimixfs_superblock *vx6_sb =
        (struct vimixfs_superblock *)first_block->data;
    if (vx6_sb->magic != VIMIXFS_MAGIC)
    {
        // wrong file system
        printk("vimixfs error: wrong file system\n");
        bio_release(first_block);
        return -EINVAL;
    }

    struct vimixfs_sb_private *priv = (struct vimixfs_sb_private *)kmalloc(
        sizeof(struct vimixfs_sb_private), ALLOC_FLAG_ZERO_MEMORY);
    if (priv == NULL)
    {
        bio_release(first_block);
        return -ENOMEM;
    }
    sb_in->s_fs_info = (void *)priv;

    memmove(&(priv->sb), vx6_sb, sizeof(struct vimixfs_superblock));
    ssize_t log_ok = log_init(&(priv->log), dev, &(priv->sb));
    bio_release(first_block);

    if (log_ok != 0)
    {
        kfree(priv);
        sb_in->s_fs_info = NULL;
        return -ENOMEM;
    }

    sb_in->s_type = &vimixfs_file_system_type;
    sb_in->s_op = &vimixfs_s_op;
    sb_in->i_op = &vimixfs_i_op;
    sb_in->f_op = &vimixfs_f_op;

    kobject_init(&sb_in->kobj, &vimixfs_kobj_ktype);
    return 0;
}

void vimixfs_kill_sb(struct super_block *sb_in)
{
    struct vimixfs_sb_private *priv =
        (struct vimixfs_sb_private *)sb_in->s_fs_info;
    DEBUG_EXTRA_ASSERT(priv != NULL, "private data should be set since mount");
    sb_in->s_fs_info = NULL;
    log_deinit(&priv->log);
    kfree(priv);
}

struct inode *vimixfs_iops_create_internal(struct inode *iparent,
                                           const char name[NAME_MAX],
                                           mode_t mode, int32_t flags,
                                           dev_t device)
{
    // if the inode already exists, return it
    inode_lock(iparent);

    // create new inode
    struct inode *ip = vimixfs_sops_alloc_inode(iparent->i_sb, mode);
    if (ip == NULL)
    {
        inode_unlock(iparent);
        return NULL;
    }

    inode_lock(ip);

    if (ip->i_sb->i_op == NULL)
    {
        panic("vimixfs_iops_create_internal: filesystem has no iop");
    }

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
    struct process *proc = get_current();
    ip->uid = proc->cred.euid;
    ip->gid = proc->cred.egid;
    vimixfs_sops_write_inode(ip);

    if (S_ISDIR(mode))
    {
        // Create . and .. entries.
        // No ip->nlink++ for ".": avoid cyclic ref count.
        if (vimixfs_dir_link_unchecked(ip, ".", ip->inum) < 0 ||
            vimixfs_dir_link_unchecked(ip, "..", iparent->inum) < 0)
        {
            goto fail;
        }
    }

    // caller checked that the file does not exist while holding dir lock.
    if (vimixfs_dir_link_unchecked(iparent, name, ip->inum) < 0)
    {
        goto fail;
    }

    if (S_ISDIR(mode))
    {
        // now that success is guaranteed:
        iparent->nlink++;  // for ".."
        vimixfs_sops_write_inode(iparent);
    }

    inode_unlock(iparent);

    return ip;

fail:
    // something went wrong. de-allocate ip.
    ip->nlink = 0;
    vimixfs_sops_write_inode(ip);
    inode_unlock_put(ip);
    inode_unlock(iparent);
    return NULL;
}

syserr_t vimixfs_fops_open(struct inode *ip, struct file *f)
{
    if (S_ISREG(ip->i_mode) && (f->flags & O_TRUNC))
    {
        // truncate if needed
        log_begin_fs_transaction(ip->i_sb);
        // lock after starting FS transaction to avoid deadlock
        // test above only read static data of the inode
        inode_lock(ip);
        vimixfs_trunc(ip, 0);
        vimixfs_sops_write_inode(ip);
        inode_unlock(ip);
        log_end_fs_transaction(ip->i_sb);
    }

    return 0;
}

syserr_t vimixfs_iops_create(struct inode *parent, struct dentry *dp,
                             mode_t mode, int32_t flags)
{
    log_begin_fs_transaction(parent->i_sb);
    struct inode *ip = vimixfs_iops_create_internal(parent, dp->name, mode,
                                                    flags, INVALID_DEVICE);

    log_end_fs_transaction(parent->i_sb);

    if (ip == NULL)
    {
        return -EFAULT;
    }

    dcache_write_lock();
    dp->ip = inode_get(ip);
    dcache_write_unlock();

    inode_unlock_put(ip);

    return (ip == NULL) ? -EFAULT : 0;
}

syserr_t vimixfs_iops_mknod(struct inode *parent, struct dentry *dp,
                            mode_t mode, dev_t dev)
{
    log_begin_fs_transaction(parent->i_sb);
    struct inode *ip =
        vimixfs_iops_create_internal(parent, dp->name, mode, 0, dev);
    log_end_fs_transaction(parent->i_sb);

    if (ip == NULL)
    {
        return -EFAULT;
    }

    dcache_write_lock();
    dp->ip = inode_get(ip);
    dcache_write_unlock();

    inode_unlock_put(ip);

    return (ip == NULL) ? -EFAULT : 0;
}

syserr_t vimixfs_iops_mkdir(struct inode *parent, struct dentry *dp,
                            mode_t mode)
{
    log_begin_fs_transaction(parent->i_sb);
    struct inode *ip =
        vimixfs_iops_create_internal(parent, dp->name, mode, 0, INVALID_DEVICE);
    log_end_fs_transaction(parent->i_sb);

    if (ip == NULL)
    {
        return -EFAULT;
    }

    dcache_write_lock();
    dp->ip = inode_get(ip);
    dcache_write_unlock();

    inode_unlock_put(ip);

    return (ip == NULL) ? -EFAULT : 0;
}

struct inode *vimixfs_sops_alloc_inode(struct super_block *sb, mode_t mode)
{
    struct vimixfs_sb_private *priv =
        (struct vimixfs_sb_private *)sb->s_fs_info;
    struct vimixfs_superblock *vsb = &(priv->sb);

    for (ino_t bnum = 0; bnum < vsb->ninode_blocks; bnum++)
    {
        struct buf *bp = bio_read(sb->dev, vsb->inodestart + bnum);

        for (ino_t inum_local = 0; inum_local < VIMIXFS_INODES_PER_BLOCK;
             inum_local++)
        {
            ino_t inum = VIMIXFS_INUM_OF_DINODE(bnum, inum_local);
            if (inum == 0)
            {
                // skip inode 0
                continue;
            }

            struct vimixfs_dinode *dip =
                (struct vimixfs_dinode *)bp->data + inum_local;

            if (dip->mode == VIMIXFS_INVALID_MODE)
            {
                // a free inode
                struct timespec time = rtc_get_time();
                memset(dip, 0, sizeof(*dip));
                dip->mode = mode;
                dip->dev = INVALID_DEVICE;
                dip->ctime = dip->mtime = time.tv_sec;
                log_write(&(priv->log), bp);  // mark it allocated on the disk
                bio_release(bp);
                return vimixfs_iget(sb, inum);
            }
        }

        bio_release(bp);
    }

    return NULL;
}

int vimixfs_sops_write_inode(struct inode *ip)
{
    struct vimixfs_sb_private *priv =
        (struct vimixfs_sb_private *)ip->i_sb->s_fs_info;
    struct vimixfs_superblock *vsb = &(priv->sb);

    size_t block_of_inode = VIMIXFS_BLOCK_OF_INODE_P(ip->inum, vsb);

    struct buf *bp = bio_read(ip->i_sb->dev, block_of_inode);
    struct vimixfs_dinode *dip = VIMIXFS_GET_DINODE(bp, ip->inum);
    dip->mode = ip->i_mode;

    if (ip->dev == ip->i_sb->dev)
    {
        // map whatever device the filesystem is on to 0
        dip->dev = INVALID_DEVICE;
    }
    else
    {
        dip->dev = ip->dev;
    }

    dip->nlink = ip->nlink;
    dip->size = ip->size;
    dip->uid = ip->uid;
    dip->gid = ip->gid;
    dip->ctime = ip->ctime;
    dip->mtime = ip->mtime;
    struct vimixfs_inode *xv_ip = vimixfs_inode_from_inode(ip);
    memmove(dip->addrs, xv_ip->addrs, sizeof(xv_ip->addrs));
    log_write(&(priv->log), bp);
    bio_release(bp);

    return 0;
}

syserr_t vimix_sops_statvfs(struct super_block *sb, struct statvfs *to_fill)
{
    DEBUG_EXTRA_PANIC(sb != NULL && to_fill != NULL,
                      "vimix_sops_statvfs: NULL pointers given");

    struct vimixfs_sb_private *priv =
        (struct vimixfs_sb_private *)sb->s_fs_info;
    struct vimixfs_superblock *vsb = &(priv->sb);

    to_fill->f_bsize = BLOCK_SIZE;
    to_fill->f_frsize = BLOCK_SIZE;
    to_fill->f_blocks = vsb->size;  // total data blocks in file system
    // count free blocks
    uint32_t free_blocks = 0;
    for (uint32_t b = 0; b < vsb->size; b += VIMIXFS_BMAP_BITS_PER_BLOCK)
    {
        struct buf *bp =
            bio_read(sb->dev, VIMIXFS_BMAP_BLOCK_OF_BIT(b, vsb->bmapstart));
        for (uint32_t bi = 0;
             bi < VIMIXFS_BMAP_BITS_PER_BLOCK && b + bi < vsb->size; bi++)
        {
            uint32_t m = 1 << (bi % 8);
            if ((bp->data[bi / 8] & m) == 0)
            {  // Is block free?
                free_blocks++;
            }
        }
        bio_release(bp);
    }
    to_fill->f_bfree = free_blocks;   // free blocks in fs
    to_fill->f_bavail = free_blocks;  // free blocks for unprivileged users
    to_fill->f_files =
        vsb->ninode_blocks *
        VIMIXFS_INODES_PER_BLOCK;  // total file nodes in file system

    // count free inodes
    uint32_t free_inodes = 0;
    for (ino_t bnum = 0; bnum < vsb->ninode_blocks; bnum++)
    {
        struct buf *bp = bio_read(sb->dev, vsb->inodestart + bnum);

        for (ino_t inum_local = 0; inum_local < VIMIXFS_INODES_PER_BLOCK;
             inum_local++)
        {
            ino_t inum = VIMIXFS_INUM_OF_DINODE(bnum, inum_local);
            if (inum == 0)
            {
                // skip inode 0
                continue;
            }

            struct vimixfs_dinode *dip = VIMIXFS_GET_DINODE(bp, inum);

            if (dip->mode == VIMIXFS_INVALID_MODE)
            {
                // a free inode
                free_inodes++;
            }
        }
        bio_release(bp);
    }
    to_fill->f_ffree = free_inodes;   // free file nodes in fs
    to_fill->f_favail = free_inodes;  // free file nodes for unprivileged users
    to_fill->f_fsid = sb->dev;        // file system id
    to_fill->f_flag = sb->s_mountflags;  // mount flags
    to_fill->f_namemax = NAME_MAX;       // maximum filename length

    return 0;
}

void vimixfs_read_inode_metadata(struct inode *ip)
{
    struct vimixfs_superblock *vsb =
        &((struct vimixfs_sb_private *)ip->i_sb->s_fs_info)->sb;

    size_t block_of_inode = VIMIXFS_BLOCK_OF_INODE_P(ip->inum, vsb);

    struct buf *bp = bio_read(ip->i_sb->dev, block_of_inode);

    struct vimixfs_dinode *dip = VIMIXFS_GET_DINODE(bp, ip->inum);
    ip->i_mode = dip->mode;

    if (dip->dev == INVALID_DEVICE)
    {
        ip->dev = ip->i_sb->dev;  // unmap device 0 to whatever device the
                                  // filesystem is on
    }
    else
    {
        ip->dev = dip->dev;
    }

    ip->nlink = dip->nlink;
    ip->size = dip->size;
    ip->uid = dip->uid;
    ip->gid = dip->gid;
    ip->ctime = dip->ctime;
    ip->mtime = dip->mtime;
    struct vimixfs_inode *xv_ip = vimixfs_inode_from_inode(ip);
    memmove(xv_ip->addrs, dip->addrs, sizeof(xv_ip->addrs));
    bio_release(bp);
}

/// @brief Truncate blocks in the given address array starting from
/// first_trunc_block to the end of the array.
/// The array can be either the direct blocks array in the VIMIX inode,
/// or an indirect block / second level of double-indirect block.
/// @param ip Inode to truncate.
/// @param addr Array of block addresses to truncate.
/// @param arr_size Full size of the addr array.
/// @param first_trunc_block First block to truncate, previous blocks are kept.
/// Can be larger than the array size (then nothing is truncated).
void vimixfs_trunc_block_range(struct inode *ip, uint32_t *addr,
                               size_t arr_size, size_t first_trunc_block)
{
    for (size_t i = first_trunc_block; i < arr_size; i++)
    {
        if (addr[i])
        {
            block_free(ip->i_sb, addr[i]);
            addr[i] = 0;
        }
    }
}

void vimixfs_trunc_block(struct inode *ip, uint32_t block_number,
                         size_t first_trunc_block)
{
    if (first_trunc_block >= VIMIXFS_N_INDIRECT_BLOCKS)
    {
        // nothing to truncate
        return;
    }

    struct buf *bp = bio_read(ip->dev, block_number);

    vimixfs_trunc_block_range(ip, (uint32_t *)bp->data,
                              VIMIXFS_N_INDIRECT_BLOCKS, first_trunc_block);
    bio_release(bp);
    if (first_trunc_block == 0)
    {
        block_free(ip->i_sb, block_number);
    }
}

static inline size_t sub_clamped(size_t val, size_t sub)
{
    return (val > sub) ? (val - sub) : 0;
}

void vimixfs_trunc(struct inode *ip, size_t first_trunc_block)
{
    struct vimixfs_inode *xv_ip = vimixfs_inode_from_inode(ip);

    // truncate direct blocks
    vimixfs_trunc_block_range(ip, xv_ip->addrs, VIMIXFS_N_DIRECT_BLOCKS,
                              first_trunc_block);
    // tracks the first block to truncate relative to the next array
    // -> subract the number of blocks in the previous array but clamp to 0
    first_trunc_block = sub_clamped(first_trunc_block, VIMIXFS_N_DIRECT_BLOCKS);

    // truncate indirect blocks
    if (xv_ip->addrs[VIMIXFS_INDIRECT_BLOCK_IDX])
    {
        vimixfs_trunc_block(ip, xv_ip->addrs[VIMIXFS_INDIRECT_BLOCK_IDX],
                            first_trunc_block);
        if (first_trunc_block == 0)
        {
            xv_ip->addrs[VIMIXFS_INDIRECT_BLOCK_IDX] = 0;
        }
        first_trunc_block =
            sub_clamped(first_trunc_block, VIMIXFS_N_INDIRECT_BLOCKS);

        // truncate double indirect blocks
        if (xv_ip->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX])
        {
            struct buf *bp = bio_read(
                ip->dev, xv_ip->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX]);
            uint32_t *indirect_block = (uint32_t *)bp->data;

            // first indirect block to truncate
            size_t first_db = first_trunc_block / VIMIXFS_N_INDIRECT_BLOCKS;
            size_t db_index = first_trunc_block % VIMIXFS_N_INDIRECT_BLOCKS;

            bool clear_indirect_block = (first_db == 0);

            if (db_index != 0)
            {
                vimixfs_trunc_block(ip, indirect_block[first_db], db_index);
                first_db++;
            }

            while ((first_db < VIMIXFS_N_INDIRECT_BLOCKS) &&
                   indirect_block[first_db])
            {
                vimixfs_trunc_block(ip, indirect_block[first_db], 0);
                indirect_block[first_db] = 0;

                first_db++;
            }

            bio_release(bp);

            if (clear_indirect_block)
            {
                block_free(ip->i_sb,
                           xv_ip->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX]);
                xv_ip->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX] = 0;
            }
        }
    }

    ip->size = first_trunc_block * BLOCK_SIZE;
}

struct inode *vimixfs_iget_locked(struct super_block *sb, ino_t inum)
{
    struct list_head *pos;
    list_for_each(pos, &sb->fs_inode_list)
    {
        struct inode *ip = inode_from_list(pos);
        if (ip->inum == inum && ip->i_sb->dev == sb->dev)
        {
            inode_get(ip);
            return ip;
        }
    }
    return NULL;
}

struct inode *vimixfs_iget(struct super_block *sb, ino_t inum)
{
    if (sb == NULL) return NULL;

    // return existing inode if it exists in the list
    rwspin_read_lock(&sb->fs_inode_list_lock);
    struct inode *ip = vimixfs_iget_locked(sb, inum);
    rwspin_read_unlock(&sb->fs_inode_list_lock);
    if (ip)
    {
        return ip;  // found existing inode
    }

    // create new inode
    // Reading the metadata from disk will sleep, so we cannot hold any locks
    // here.
    struct vimixfs_inode *xv_ip =
        kmalloc(sizeof(struct vimixfs_inode), ALLOC_FLAG_ZERO_MEMORY);
    if (xv_ip == NULL)
    {
        return NULL;  // out of memory
    }
    ip = &xv_ip->ino;
    inode_init(ip, sb, inum);
    // read metadata from disk
    vimixfs_read_inode_metadata(ip);

    // add to inode list
    rwspin_write_lock(&sb->fs_inode_list_lock);
    // worst case someone else created it in the meantime
    struct inode *ip_check = vimixfs_iget_locked(sb, inum);
    if (ip_check)
    {
        // another thread created it in the meantime
        rwspin_write_unlock(&sb->fs_inode_list_lock);
        kfree(xv_ip);  // kfree is enough, inode was not fully initialized
                       // before adding to the list
        return ip_check;
    }
    list_add_tail(&ip->fs_inode_list, &sb->fs_inode_list);
    rwspin_write_unlock(&sb->fs_inode_list_lock);

    return ip;
}

void vimixfs_iops_put(struct inode *ip)
{
    bool free = kref_put(&ip->ref);
    if (free == false)
    {
        // still referenced
        return;
    }

    // last reference dropped
    // get a write lock on the inode list to prevent anyone else from
    // getting this inode while we are deleting it
    struct rwspinlock *list_lock = &ip->i_sb->fs_inode_list_lock;
    rwspin_write_lock(list_lock);
    // check again, maybe another thread got it in the meantime
    if (kref_read(&ip->ref) != 0)
    {
        rwspin_write_unlock(list_lock);
        return;  // someone else got a reference in the meantime
    }
    inode_del(ip);  // removes from inode list -> can't get discovered anymore
    rwspin_write_unlock(list_lock);

    // If the inode has no links and no other references: truncate and free
    // on disk.
    if (ip->nlink == 0)
    {
        struct process *proc = get_current();
        bool external_fs_transaction = (proc->debug_log_depth != 0);

        if (!external_fs_transaction)
        {
            // rare case: e.g. a file was deleted while someone still had a
            // reference (namex() during traversal?). Now the inode_put() of
            // the second process will trigger the delete on the FS. This is
            // fine and might happen inside of the FS transaction of another
            // FS syscall, but if it doesn't, a new FS transaction must be
            // started. Otherwise this might also trigger an error if no
            // other FS transaction is active...

            log_begin_fs_transaction(ip->i_sb);
        }

        // ip->ref == 0 means no other process can have ip locked,
        // so this sleep_lock() won't block (or deadlock).
        sleep_lock(&ip->lock);

        vimixfs_trunc(ip, 0);
        ip->i_mode = 0;
        vimixfs_sops_write_inode(ip);

        sleep_unlock(&ip->lock);

        if (!external_fs_transaction)
        {
            log_end_fs_transaction(ip->i_sb);
        }
    }

    kfree(vimixfs_inode_from_inode(ip));
}

bool inode_is_mounted_fs_root(struct inode *dir)
{
    return ((dir == dir->i_sb->s_root) && (dir->i_sb->imounted_on));
}

struct inode *vimixfs_lookup_old(struct inode *dir, const char *name,
                                 uint32_t *poff)
{
    struct vimixfs_dirent de;
    for (size_t off = 0; off < dir->size; off += sizeof(de))
    {
        if (vimixfs_iops_read(dir, off, (size_t)&de, sizeof(de), false) !=
            sizeof(de))
        {
            panic("vimixfs_lookup read error");
        }
        if (de.inum == INVALID_INODE)
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
            // if (inode_is_mounted_fs_root(dir) &&
            //     (file_name_cmp("..", de.name) == 0))
            //{
            //     inode_lock(dir->i_sb->imounted_on);
            //     struct inode *ret =
            //         VFS_INODE_LOOKUP(dir->i_sb->imounted_on, "..", poff);
            //     inode_unlock(dir->i_sb->imounted_on);
            //     return ret;
            // }
            return vimixfs_iget(dir->i_sb, (ino_t)de.inum);
        }
    }

    return NULL;
}

struct inode *vimixfs_lookup(struct inode *dir, const char *name,
                             uint32_t *poff)
{
    size_t block_count = (dir->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    const size_t DIRS_PER_BLOCK = BLOCK_SIZE / sizeof(struct vimixfs_dirent);

    for (size_t block = 0; block < block_count; block++)
    {
        size_t addr = bmap_get_block_address(dir, block);
        if (addr == 0)
        {
            return NULL;
        }
        struct buf *bp = bio_read(dir->dev, addr);

        for (size_t dir_idx = 0; dir_idx < DIRS_PER_BLOCK; dir_idx++)
        {
            uint32_t dir_off =
                (uint32_t)(dir_idx * sizeof(struct vimixfs_dirent) +
                           block * BLOCK_SIZE);
            if (dir_off >= dir->size)
            {
                bio_release(bp);
                return NULL;
            }

            struct vimixfs_dirent *de =
                (struct vimixfs_dirent
                     *)(&bp->data[dir_idx * sizeof(struct vimixfs_dirent)]);

            if (de->inum == INVALID_INODE)
            {
                continue;
            }

            if (file_name_cmp(name, de->name) == 0)
            {
                if (poff)
                {
                    *poff = dir_off;
                }
                // entry matches path element
                struct inode *ip = vimixfs_iget(dir->i_sb, (ino_t)de->inum);
                bio_release(bp);
                return ip;
            }
        }

        bio_release(bp);
    }

    return NULL;
}

struct dentry *vimixfs_iops_lookup(struct inode *parent, struct dentry *dp)
{
    dp->ip = vimixfs_lookup(parent, dp->name, NULL);
    return dp;
}

syserr_t vimixfs_dir_link(struct inode *dir, const char *name, ino_t inum)
{
    // Check that name is not present.
    struct inode *ip = vimixfs_lookup(dir, name, NULL);
    if (ip != NULL)
    {
        inode_put(ip);
        return -EEXIST;
    }

    return vimixfs_dir_link_unchecked(dir, name, inum);
}

syserr_t vimixfs_dir_link_unchecked_old(struct inode *dir, const char *name,
                                        ino_t inum)
{
    // Look for an empty vimixfs_dirent.
    struct vimixfs_dirent de;
    size_t off;
    for (off = 0; off < dir->size; off += sizeof(de))
    {
        ssize_t read =
            vimixfs_iops_read(dir, off, (size_t)&de, sizeof(de), false);
        if (read != sizeof(de))
        {
            panic("vimixfs_dir_link read wrong amount of data");
        }
        if (de.inum == INVALID_INODE)
        {
            break;
        }
    }

    strncpy(de.name, name, VIMIXFS_NAME_MAX);
    de.inum = (uint32_t)inum;

    ssize_t written = vimixfs_write(dir, false, (size_t)&de, off, sizeof(de));
    if (written != sizeof(de))
    {
        return -EOTHER;
    }

    return 0;
}

syserr_t vimixfs_dir_link_unchecked(struct inode *dir, const char *name,
                                    ino_t inum)
{
    size_t block_count = (dir->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    const size_t DIRS_PER_BLOCK = BLOCK_SIZE / sizeof(struct vimixfs_dirent);

    // look for a free dirent
    for (size_t block = 0; block < block_count; block++)
    {
        size_t addr = bmap_get_block_address(dir, block);
        if (addr == 0)
        {
            return -EOTHER;
        }
        struct buf *bp = bio_read(dir->dev, addr);

        for (size_t dir_idx = 0; dir_idx < DIRS_PER_BLOCK; dir_idx++)
        {
            uint32_t dir_off =
                (uint32_t)(dir_idx * sizeof(struct vimixfs_dirent) +
                           block * BLOCK_SIZE);
            if (dir_off >= dir->size)
            {
                break;
            }

            struct vimixfs_dirent *de =
                (struct vimixfs_dirent
                     *)(&bp->data[dir_idx * sizeof(struct vimixfs_dirent)]);

            if (de->inum == INVALID_INODE)
            {
                // found free entry
                strncpy(de->name, name, VIMIXFS_NAME_MAX);
                de->inum = (uint32_t)inum;

                struct vimixfs_sb_private *priv =
                    (struct vimixfs_sb_private *)dir->i_sb->s_fs_info;
                log_write(&(priv->log), bp);
                bio_release(bp);
                return 0;
            }
        }
        bio_release(bp);
    }

    // none found, try to increase dir size
    struct vimixfs_dirent de;
    strncpy(de.name, name, VIMIXFS_NAME_MAX);
    de.inum = (uint32_t)inum;

    ssize_t written =
        vimixfs_write(dir, false, (size_t)&de, dir->size, sizeof(de));
    if (written != sizeof(de))
    {
        return -EOTHER;
    }

    return 0;
}

syserr_t vimixfs_iops_get_dirent(struct inode *dir, struct dirent *dir_entry,
                                 ssize_t seek_pos)
{
    struct vimixfs_dirent vimixfs_dir_entry;
    inode_lock(dir);
    ssize_t new_seek_pos = seek_pos;

    do
    {
        size_t read_bytes;
        read_bytes = vimixfs_iops_read(dir, (size_t)new_seek_pos,
                                       (size_t)&vimixfs_dir_entry,
                                       sizeof(struct vimixfs_dirent), false);
        if (read_bytes <= 0)
        {
            inode_unlock(dir);
            return read_bytes;  // 0 if no more dirents to read or -1
                                // on error
        }
        else if (read_bytes < sizeof(struct vimixfs_dirent))
        {
            inode_unlock(dir);
            return 0;
        }
        new_seek_pos += read_bytes;
    } while (vimixfs_dir_entry.inum == INVALID_INODE);  // skip unused entries

    inode_unlock(dir);

    dir_entry->d_ino = vimixfs_dir_entry.inum;
    dir_entry->d_reclen = sizeof(struct dirent);
    strncpy(dir_entry->d_name, vimixfs_dir_entry.name, VIMIXFS_NAME_MAX);
    dir_entry->d_off = (long)(new_seek_pos);

    return (syserr_t)new_seek_pos;
}

syserr_t vimixfs_iops_read(struct inode *ip, size_t off, size_t dst, size_t n,
                           bool addr_is_userspace)
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
        size_t addr = bmap_get_block_address(ip, off / BLOCK_SIZE);
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

syserr_t vimixfs_write(struct inode *ip, bool src_addr_is_userspace, size_t src,
                       size_t off, size_t n)
{
    if (off > ip->size || off + n < off)
    {
        return -EINVAL;
    }

    if (off + n > VIMIXFS_MAX_FILE_SIZE_BLOCKS * BLOCK_SIZE)
    {
        return -EINVAL;
    }

    ssize_t m = 0;
    syserr_t tot;
    for (tot = 0; tot < n; tot += m, off += m, src += m)
    {
        size_t addr = bmap_get_block_address(ip, off / BLOCK_SIZE);
        if (addr == 0)
        {
            break;
        }
        struct buf *bp = bio_read(ip->dev, addr);
        m = min(n - tot, BLOCK_SIZE - off % BLOCK_SIZE);

        if (either_copyin(bp->data + (off % BLOCK_SIZE), src_addr_is_userspace,
                          src, m) < 0)
        {
            bio_release(bp);
            tot = -EFAULT;
            break;
        }
        struct vimixfs_sb_private *priv =
            (struct vimixfs_sb_private *)ip->i_sb->s_fs_info;
        log_write(&(priv->log), bp);
        bio_release(bp);
    }

    if (off > ip->size)
    {
        ip->size = off;
    }

    // write the i-node back to disk even if the size didn't change
    // because the loop above might have called bmap_get_block_address() and
    // added a new block to ip->addrs[].
    vimixfs_sops_write_inode(ip);

    return tot;
}

syserr_t vimixfs_iops_link(struct dentry *file_from, struct inode *dir_to,
                           struct dentry *new_link)
{
    struct super_block *sb = dir_to->i_sb;
    log_begin_fs_transaction(sb);
    inode_lock_2(dir_to, file_from->ip);

    file_from->ip->nlink++;
    vimixfs_sops_write_inode(file_from->ip);
    inode_unlock(file_from->ip);

    if (vimixfs_dir_link(dir_to, new_link->name, file_from->ip->inum) < 0)
    {
        inode_unlock(dir_to);

        inode_lock(file_from->ip);
        file_from->ip->nlink--;
        vimixfs_sops_write_inode(file_from->ip);
        inode_unlock(file_from->ip);
        log_end_fs_transaction(sb);
        return -EOTHER;
    }
    else
    {
        // success
        new_link->ip = inode_get(file_from->ip);
    }
    log_end_fs_transaction(sb);

    inode_unlock(dir_to);

    return 0;
}

syserr_t vimixfs_fops_write(struct file *f, size_t addr, size_t n)
{
    // -1; inode
    // -1; unaligned writes
    // -1; if new indirect block
    // OR -2; if new double indirect block
    // -1; for additional indirect block if write crosses to new block
    const size_t extra_blocks = 5;

    struct inode *ip = f->dp->ip;

    ssize_t written_total = 0;
    while (written_total < n)
    {
        ssize_t to_write = n - written_total;
        size_t to_write_blocks =
            (to_write + BLOCK_SIZE - 1) / BLOCK_SIZE;  // round up

        size_t client = log_begin_fs_transaction_explicit(
            ip->i_sb, 1 + extra_blocks, to_write_blocks + extra_blocks);
        size_t reserved = log_get_client_available_blocks(ip->i_sb, client);
        size_t max_bytes = (reserved - extra_blocks) * BLOCK_SIZE;

        if (to_write > max_bytes)
        {
            to_write = max_bytes;
        }

        inode_lock(ip);

        syserr_t bytes_written =
            vimixfs_write(ip, true, addr + written_total, f->off, to_write);
        if (bytes_written > 0)
        {
            f->off += bytes_written;
            written_total += bytes_written;
        }
        inode_unlock(ip);
        log_end_fs_transaction(ip->i_sb);

        if (bytes_written != to_write)
        {
            if (bytes_written < 0)
            {
                return bytes_written;  // return error code
            }
            return bytes_written;
        }
    }

    return written_total;  // should be == n
}

/// Is the directory dir empty except for "." and ".." ?
static int isdirempty(struct inode *dir)
{
    struct vimixfs_dirent de;

    for (size_t off = 2 * sizeof(de); off < dir->size; off += sizeof(de))
    {
        if (vimixfs_iops_read(dir, off, (size_t)&de, sizeof(de), false) !=
            sizeof(de))
        {
            panic("isdirempty: vimixfs_iops_read");
        }

        if (de.inum != INVALID_INODE)
        {
            return 0;
        }
    }
    return 1;
}

syserr_t vimixfs_iops_unlink(struct inode *parent, struct dentry *dp)
{
    // save sb pointer in case dir is freed
    struct super_block *sb = parent->i_sb;
    log_begin_fs_transaction(sb);
    inode_lock(parent);
    uint32_t off;
    struct inode *ip = vimixfs_lookup(parent, dp->name, &off);
    if (ip == NULL)
    {
        inode_unlock(parent);
        log_end_fs_transaction(sb);
        return -ENOENT;
    }
    inode_lock(ip);

    if (ip->nlink < 1)
    {
        panic("unlink: nlink < 1");
    }

    if (S_ISDIR(ip->i_mode))
    {
        inode_unlock_put(ip);
        inode_unlock(parent);
        log_end_fs_transaction(sb);
        return -EISDIR;
    }

    // delete directory entry by over-writing it with zeros:
    struct vimixfs_dirent de;
    memset(&de, 0, sizeof(de));
    if (vimixfs_write(parent, false, (size_t)&de, off, sizeof(de)) !=
        sizeof(de))
    {
        panic("vimixfs_iops_unlink: vimixfs_write");
    }

    inode_unlock(parent);

    ip->nlink--;
    vimixfs_sops_write_inode(ip);
    inode_unlock_put(ip);

    log_end_fs_transaction(sb);

    return 0;
}

syserr_t vimixfs_iops_rmdir(struct inode *parent, struct dentry *dp)
{
    // save sb pointer in case dir is freed
    struct super_block *sb = parent->i_sb;
    log_begin_fs_transaction(sb);
    inode_lock(parent);
    uint32_t off;
    struct inode *ip = vimixfs_lookup(parent, dp->name, &off);
    if (ip == NULL)
    {
        inode_unlock(parent);
        log_end_fs_transaction(sb);
        return -ENOENT;
    }
    DEBUG_EXTRA_PANIC(ip == dp->ip,
                      "vimixfs_iops_rmdir: dentry inode mismatch");
    inode_lock(ip);

    if (ip->nlink < 1)
    {
        panic("unlink: nlink < 1");
    }

    if (!isdirempty(ip))
    {
        inode_unlock_put(ip);
        inode_unlock(parent);
        log_end_fs_transaction(sb);
        return -ENOTEMPTY;
    }

    // delete directory entry by over-writing it with zeros:
    struct vimixfs_dirent de;
    memset(&de, 0, sizeof(de));
    if (vimixfs_write(parent, false, (size_t)&de, off, sizeof(de)) !=
        sizeof(de))
    {
        panic("vimixfs_iops_unlink: vimixfs_write");
    }

    inode_unlock(parent);

    ip->nlink--;
    vimixfs_sops_write_inode(ip);
    inode_unlock_put(ip);

    log_end_fs_transaction(sb);

    return 0;
}

/// @brief Clear (set to zero) the data in block_number starting from from_byte
/// to the end of the block.
/// @param ip Inode of the file.
/// @param block_number Existing block number in the file.
/// @param from_byte Starting byte inside of the block to clear
/// [0..BLOCK_SIZE-1].
void clear_block_from(struct inode *ip, size_t block_number, size_t from_byte)
{
    if (from_byte >= BLOCK_SIZE)
    {
        return;
    }

    size_t addr = bmap_get_block_address(ip, block_number);
    DEBUG_EXTRA_PANIC(addr != 0,
                      "clear_block_from: bmap_get_block_address failed");

    struct buf *bp = bio_read(ip->dev, addr);
    memset(bp->data + from_byte, 0, BLOCK_SIZE - from_byte);

    struct vimixfs_sb_private *priv =
        (struct vimixfs_sb_private *)ip->i_sb->s_fs_info;
    log_write(&(priv->log), bp);
    bio_release(bp);
}

ssize_t trunc_shrink(struct inode *ip, ssize_t new_size, size_t client,
                     size_t MIN_BLOCKS)
{
    while (new_size < ip->size)
    {
        if (ip->size % BLOCK_SIZE != 0)
        {
            size_t clear_block = ip->size / BLOCK_SIZE;
            size_t clear_block_start = clear_block * BLOCK_SIZE;

            if (new_size >= clear_block_start)
            {
                // same number of blocks, just clear rest of last block
                size_t clear_start = new_size % BLOCK_SIZE;
                clear_block_from(ip, clear_block, clear_start);
                ip->size = new_size;
                return 0;
            }

            // remove full last block
            vimixfs_trunc(ip, clear_block);
            ip->size = clear_block_start;
        }
        else
        {
            // size_t diff = ip->size - new_size;
            // size_t diff_blocks = diff / BLOCK_SIZE;
            //
            // if (diff_blocks > 1)
            //{
            //    // remove full blocks
            //    size_t first_trunc_block = ip->size / BLOCK_SIZE -
            //    diff_blocks; vimixfs_trunc(ip, first_trunc_block); ip->size =
            //    first_trunc_block * BLOCK_SIZE;
            //}

            size_t clear_block = ip->size / BLOCK_SIZE - 1;
            size_t clear_block_start = clear_block * BLOCK_SIZE;
            ssize_t clear_start = max(clear_block_start, new_size);

            if (clear_start % BLOCK_SIZE != 0)
            {
                clear_block_from(ip, clear_block, clear_start);
            }
            else
            {
                // remove full last block
                vimixfs_trunc(ip, clear_block);
            }

            ip->size = clear_start;
        }

        size_t available_blocks =
            log_get_client_available_blocks(ip->i_sb, client);
        if (available_blocks < MIN_BLOCKS) break;
    }
    return 0;
}
ssize_t trunc_grow(struct inode *ip, ssize_t new_size, size_t client,
                   size_t MIN_BLOCKS)
{
    while (new_size > ip->size)
    {
        if ((ip->size / BLOCK_SIZE) == (new_size / BLOCK_SIZE))
        {
            // same number of blocks, just to be sure zero rest of last
            // block
            size_t clear_start = ip->size % BLOCK_SIZE;
            clear_block_from(ip, ip->size / BLOCK_SIZE, clear_start);
            ip->size = new_size;
        }

        if (ip->size % BLOCK_SIZE != 0)
        {
            // clear remainder of already allocated block
            size_t clear_start = ip->size % BLOCK_SIZE;
            clear_block_from(ip, ip->size / BLOCK_SIZE, clear_start);
            size_t max_size = ((ip->size / BLOCK_SIZE) + 1) * BLOCK_SIZE;

            ip->size = min(max_size, new_size);
        }
        else
        {
            // need to alloc full blocks, will be zeroes already
            ssize_t next_block = ip->size / BLOCK_SIZE + 1;

            if (bmap_get_block_address(ip, next_block) == 0)
            {
                // block could not get allocated, out of space
                return -ENOSPC;
            }
            ip->size += BLOCK_SIZE;
        }

        size_t available_blocks =
            log_get_client_available_blocks(ip->i_sb, client);
        if (available_blocks < MIN_BLOCKS) break;
    }
    return 0;
}

syserr_t vimixfs_iops_truncate(struct dentry *dp, off_t new_size)
{
    if (new_size > VIMIXFS_MAX_FILE_SIZE_BLOCKS * BLOCK_SIZE)
    {
        return -EFBIG;
    }

    struct inode *ip = dp->ip;

    // worst case requirements for a size change <= 1 BLOCK_SIZE:
    const size_t MIN_BLOCKS_FOR_TRUNCATE = 5;

    // loop until we have truncated to the desired size
    // because we might need multiple FS transactions
    while (ip->size != new_size)
    {
        size_t client = log_begin_fs_transaction_explicit(
            ip->i_sb, MIN_BLOCKS_FOR_TRUNCATE, 5 * MIN_BLOCKS_FOR_TRUNCATE);

        inode_lock(ip);

        syserr_t ret = 0;
        if (new_size < ip->size)
        {
            ret = trunc_shrink(ip, new_size, client, MIN_BLOCKS_FOR_TRUNCATE);
        }
        else
        {
            ret = trunc_grow(ip, new_size, client, MIN_BLOCKS_FOR_TRUNCATE);
        }

        // update metadata
        vimixfs_sops_write_inode(ip);
        inode_unlock(ip);
        log_end_fs_transaction(ip->i_sb);

        if (ret < 0)
        {
            return ret;
        }

        // printk("truncated to %zd (wanted %zd)\n", file_size, new_size);
    }

    return 0;
}

syserr_t vimixfs_iops_chmod(struct dentry *dp, mode_t mode)
{
    struct inode *ip = dp->ip;

    // will only change the inode data on disk, one block
    log_begin_fs_transaction_explicit(ip->i_sb, 1, 1);

    inode_lock(ip);
    mode_t type = ip->i_mode & S_IFMT;
    ip->i_mode = mode | type;
    vimixfs_sops_write_inode(ip);
    inode_unlock(ip);

    log_end_fs_transaction(ip->i_sb);

    return 0;
}

syserr_t vimixfs_iops_chown(struct dentry *dp, uid_t uid, gid_t gid)
{
    struct inode *ip = dp->ip;

    // will only change the inode data on disk, one block
    log_begin_fs_transaction_explicit(ip->i_sb, 1, 1);

    inode_lock(ip);
    if (uid >= 0) ip->uid = uid;
    if (gid >= 0) ip->gid = gid;
    vimixfs_sops_write_inode(ip);
    inode_unlock(ip);

    log_end_fs_transaction(ip->i_sb);

    return 0;
}
