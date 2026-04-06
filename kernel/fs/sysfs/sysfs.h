/* SPDX-License-Identifier: MIT */
#pragma once

#include <fs/sysfs/sysfs_sb_priv.h>
#include <kernel/fs.h>

void sysfs_init();

struct sysfs_node *sysfs_register_kobject(struct kobject *kobj);
void sysfs_unregister_kobject(struct kobject *kobj);

syserr_t sysfs_init_fs_super_block(struct super_block *sb_in, const void *data);

void sysfs_kill_sb(struct super_block *sb_in);

struct inode *sysfs_sops_iget_root(struct super_block *sb);

struct inode *sysfs_sops_alloc_inode(struct super_block *sb, mode_t mode);

int sysfs_sops_write_inode(struct inode *ip);

void sysfs_iops_put(struct inode *ip);

struct dentry *sysfs_iops_lookup(struct inode *parent, struct dentry *dp);

syserr_t sysfs_iops_get_dirent(struct inode *dir, struct dirent *dir_entry,
                               ssize_t seek_pos);

syserr_t sysfs_iops_read(struct inode *ip, size_t off, size_t dst, size_t n,
                         bool addr_is_userspace);

syserr_t sysfs_fops_write(struct file *f, size_t addr, size_t n);
