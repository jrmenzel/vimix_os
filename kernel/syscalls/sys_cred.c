/* SPDX-License-Identifier: MIT */

//
// Credential management system calls.
//

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/process.h>
#include <syscalls/syscall.h>

syserr_t do_getresuid(size_t ruid_addr, size_t euid_addr, size_t suid_addr);
syserr_t do_getresgid(size_t rgid_addr, size_t egid_addr, size_t sgid_addr);
syserr_t do_setuid(uid_t uid);
syserr_t do_setgid(gid_t gid);
syserr_t do_setresuid(uid_t ruid, uid_t euid, uid_t suid);
syserr_t do_setresgid(gid_t rgid, gid_t egid, gid_t sgid);

syserr_t sys_getresuid()
{
    // parameter 0: uid_t *ruid
    size_t ruid_addr;
    argaddr(0, &ruid_addr);

    // parameter 1: uid_t *euid
    size_t euid_addr;
    argaddr(1, &euid_addr);

    // parameter 2: uid_t *suid
    size_t suid_addr;
    argaddr(2, &suid_addr);

    return do_getresuid(ruid_addr, euid_addr, suid_addr);
}

syserr_t sys_getresgid()
{
    // parameter 0: gid_t *rgid
    size_t rgid_addr;
    argaddr(0, &rgid_addr);

    // parameter 1: gid_t *egid
    size_t egid_addr;
    argaddr(1, &egid_addr);

    // parameter 2: gid_t *sgid
    size_t sgid_addr;
    argaddr(2, &sgid_addr);

    return do_getresgid(rgid_addr, egid_addr, sgid_addr);
}

syserr_t sys_setuid()
{
    // parameter 0: uid_t uid
    int32_t uid;
    argint(0, &uid);

    return do_setuid((uid_t)uid);
}

syserr_t sys_setgid()
{
    // parameter 0: gid_t gid
    int32_t gid;
    argint(0, &gid);

    return do_setgid((gid_t)gid);
}

syserr_t sys_setresuid()
{
    // parameter 0: uid_t ruid
    int32_t ruid;
    argint(0, &ruid);

    // parameter 1: uid_t euid
    int32_t euid;
    argint(1, &euid);

    // parameter 2: uid_t suid
    int32_t suid;
    argint(2, &suid);

    return do_setresuid((uid_t)ruid, (uid_t)euid, (uid_t)suid);
}

syserr_t sys_setresgid()
{
    // parameter 0: gid_t rgid
    int32_t rgid;
    argint(0, &rgid);

    // parameter 1: gid_t egid
    int32_t egid;
    argint(1, &egid);

    // parameter 2: gid_t sgid
    int32_t sgid;
    argint(2, &sgid);

    return do_setresgid((gid_t)rgid, (gid_t)egid, (gid_t)sgid);
}

/// Helper function to copy out up to three IDs to user space.
syserr_t copy_out_ids(struct process *proc, size_t addr_0, size_t addr_1,
                      size_t addr_2, int32_t value_0, int32_t value_1,
                      int32_t value_2)
{
    if (addr_0 != 0)
    {
        if (uvm_copy_out(proc->pagetable, addr_0, (char *)&value_0,
                         sizeof(int32_t)) < 0)
        {
            return -EFAULT;
        }
    }
    if (addr_1 != 0)
    {
        if (uvm_copy_out(proc->pagetable, addr_1, (char *)&value_1,
                         sizeof(int32_t)) < 0)
        {
            return -EFAULT;
        }
    }
    if (addr_2 != 0)
    {
        if (uvm_copy_out(proc->pagetable, addr_2, (char *)&value_2,
                         sizeof(int32_t)) < 0)
        {
            return -EFAULT;
        }
    }

    return 0;
}

syserr_t do_getresuid(size_t ruid_addr, size_t euid_addr, size_t suid_addr)
{
    struct process *proc = get_current();
    uid_t ruid = proc->cred.uid;
    uid_t euid = proc->cred.euid;
    uid_t suid = proc->cred.suid;

    return copy_out_ids(proc, ruid_addr, euid_addr, suid_addr, ruid, euid,
                        suid);
}

syserr_t do_getresgid(size_t rgid_addr, size_t egid_addr, size_t sgid_addr)
{
    struct process *proc = get_current();
    gid_t rgid = proc->cred.gid;
    gid_t egid = proc->cred.egid;
    gid_t sgid = proc->cred.sgid;

    return copy_out_ids(proc, rgid_addr, egid_addr, sgid_addr, rgid, egid,
                        sgid);
}

syserr_t do_setuid(uid_t uid)
{
    struct process *proc = get_current();

    if (IS_SUPERUSER(&proc->cred))
    {
        proc->cred.uid = (uid_t)uid;
        proc->cred.euid = (uid_t)uid;
        proc->cred.suid = (uid_t)uid;
    }
    else
    {
        // un-privileged user can only set to real, effective or saved uid
        if (uid != proc->cred.uid && uid != proc->cred.euid &&
            uid != proc->cred.suid)
        {
            return -EPERM;
        }
        proc->cred.euid = (uid_t)uid;
    }

    return 0;
}

syserr_t do_setgid(gid_t gid)
{
    struct process *proc = get_current();

    if (IS_SUPERUSER(&proc->cred))
    {
        proc->cred.gid = gid;
        proc->cred.egid = gid;
        proc->cred.sgid = gid;
    }
    else
    {
        // un-privileged user can only set to real, effective or saved uid
        if (gid != proc->cred.gid && gid != proc->cred.egid &&
            gid != proc->cred.sgid)
        {
            return -EPERM;
        }
        proc->cred.egid = gid;
    }

    return 0;
}

syserr_t do_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
    struct process *proc = get_current();

    if (IS_NOT_SUPERUSER(&proc->cred))
    {
        // un-privileged user can only set to real, effective or saved uid
        if ((ruid != -1 && ruid != proc->cred.uid && ruid != proc->cred.euid &&
             ruid != proc->cred.suid) ||
            (euid != -1 && euid != proc->cred.uid && euid != proc->cred.euid &&
             euid != proc->cred.suid) ||
            (suid != -1 && suid != proc->cred.uid && suid != proc->cred.euid &&
             suid != proc->cred.suid))
        {
            return -EPERM;
        }
    }

    if (ruid != -1) proc->cred.uid = (uid_t)ruid;
    if (euid != -1) proc->cred.euid = (uid_t)euid;
    if (suid != -1) proc->cred.suid = (uid_t)suid;

    return 0;
}

syserr_t do_setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
    struct process *proc = get_current();

    if (IS_NOT_SUPERUSER(&proc->cred))
    {
        // un-privileged user can only set to real, effective or saved gid
        if ((rgid != -1 && rgid != proc->cred.gid && rgid != proc->cred.egid &&
             rgid != proc->cred.sgid) ||
            (egid != -1 && egid != proc->cred.gid && egid != proc->cred.egid &&
             egid != proc->cred.sgid) ||
            (sgid != -1 && sgid != proc->cred.gid && sgid != proc->cred.egid &&
             sgid != proc->cred.sgid))
        {
            return -EPERM;
        }
    }

    if (rgid != -1) proc->cred.gid = (gid_t)rgid;
    if (egid != -1) proc->cred.egid = (gid_t)egid;
    if (sgid != -1) proc->cred.sgid = (gid_t)sgid;

    return 0;
}
