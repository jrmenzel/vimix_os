/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/fcntl.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/process.h>

#define MAY_EXEC 0x00000001
#define MAY_WRITE 0x00000002
#define MAY_READ 0x00000004
#define MAY_APPEND 0x00000008
#define MAY_ACCESS 0x00000010
#define MAY_OPEN 0x00000020
#define MAY_CHDIR 0x00000040
#define MAY_UNLINK 0x00000080

typedef int32_t perm_mask_t;

syserr_t check_file_permission(struct process *proc, struct file *f,
                               perm_mask_t mask);

syserr_t check_inode_permission(struct process *proc, struct inode *ip,
                                perm_mask_t mask);

syserr_t check_dentry_permission(struct process *proc, struct dentry *dp,
                                 perm_mask_t mask);

perm_mask_t perm_mask_from_open_flags(int32_t flags);

#define ASSERT_IS_FILE_OWNER(proc, ip)                                         \
    do                                                                         \
    {                                                                          \
        if (IS_NOT_SUPERUSER(&(proc)->cred) && (proc)->cred.euid != (ip)->uid) \
        {                                                                      \
            return -EPERM;                                                     \
        }                                                                      \
    } while (0)
