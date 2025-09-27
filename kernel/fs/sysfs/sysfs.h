/* SPDX-License-Identifier: MIT */
#pragma once

#include <fs/sysfs/sysfs_sb_priv.h>
#include <kernel/fs.h>

void sysfs_init();

void sysfs_register_kobject(struct kobject *kobj);
void sysfs_unregister_kobject(struct kobject *kobj);

ssize_t sysfs_init_fs_super_block(struct super_block *sb_in, const void *data);

void sysfs_kill_sb(struct super_block *sb_in);

struct inode *sysfs_sops_iget_root(struct super_block *sb);

struct inode *sysfs_sops_alloc_inode(struct super_block *sb, mode_t mode);

int sysfs_sops_write_inode(struct inode *ip);

struct inode *sysfs_iops_create(struct inode *iparent, char name[NAME_MAX],
                                mode_t mode, int32_t flags, dev_t device);

struct inode *sysfs_iops_open(struct inode *iparent, char name[NAME_MAX],
                              int32_t flags);

void sysfs_iops_read_in(struct inode *ip);

void sysfs_iops_put(struct inode *ip);

struct inode *sysfs_iops_dir_lookup(struct inode *dir, const char *name,
                                    uint32_t *poff);

int sysfs_iops_dir_link(struct inode *dir, char *name, ino_t inum);

ssize_t sysfs_iops_get_dirent(struct inode *dir, size_t dir_entry_addr,
                              bool addr_is_userspace, ssize_t seek_pos);

ssize_t sysfs_iops_read(struct inode *ip, bool addr_is_userspace, size_t dst,
                        size_t off, size_t n);

ssize_t sysfs_iops_link(struct inode *dir, struct inode *ip,
                        char name[NAME_MAX]);

ssize_t sysfs_iops_unlink(struct inode *dir, char name[NAME_MAX],
                          bool delete_files, bool delete_directories);

ssize_t sysfs_fops_write(struct file *f, size_t addr, size_t n);
