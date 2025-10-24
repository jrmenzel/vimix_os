/* SPDX-License-Identifier: MIT */

//
// Credential management system calls.
//

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/process.h>
#include <syscalls/syscall.h>

/// @brief Copy out three IDs (uid_t or gid_t) to user space if the provided
/// pointer is not NULL.
/// @param proc Calling process.
/// @param r_id Real id.
/// @param e_id Effective id.
/// @param s_id Saved id.
/// @return 0 on success, -EFAULT on error.
ssize_t getres_id(struct process *proc, int32_t r_id, int32_t e_id,
                  int32_t s_id)
{
    // parameter 0: uid_t *ruid or gid_t *rgid
    size_t rid;
    argaddr(0, &rid);

    // parameter 1: uid_t *euid or gid_t *egid
    size_t eid;
    argaddr(1, &eid);

    // parameter 2: uid_t *suid or gid_t *sgid
    size_t sid;
    argaddr(2, &sid);

    if (rid != 0)
    {
        if (uvm_copy_out(proc->pagetable, rid, (char *)&r_id, sizeof(int32_t)) <
            0)
        {
            return -EFAULT;
        }
    }
    if (eid != 0)
    {
        if (uvm_copy_out(proc->pagetable, eid, (char *)&e_id, sizeof(int32_t)) <
            0)
        {
            return -EFAULT;
        }
    }
    if (sid != 0)
    {
        if (uvm_copy_out(proc->pagetable, sid, (char *)&s_id, sizeof(int32_t)) <
            0)
        {
            return -EFAULT;
        }
    }

    return 0;
}

ssize_t sys_getresuid()
{
    struct process *proc = get_current();
    return getres_id(proc, proc->cred.uid, proc->cred.euid, proc->cred.suid);
}

ssize_t sys_getresgid()
{
    struct process *proc = get_current();
    return getres_id(proc, proc->cred.gid, proc->cred.egid, proc->cred.sgid);
}

ssize_t sys_setuid()
{
    struct process *proc = get_current();

    // parameter 0: uid_t uid
    int32_t uid;
    argint(0, &uid);

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

ssize_t sys_setgid()
{
    struct process *proc = get_current();

    // parameter 0: gid_t gid
    int32_t gid;
    argint(0, &gid);

    if (IS_SUPERUSER(&proc->cred))
    {
        proc->cred.gid = (gid_t)gid;
        proc->cred.egid = (gid_t)gid;
        proc->cred.sgid = (gid_t)gid;
    }
    else
    {
        // un-privileged user can only set to real, effective or saved uid
        if (gid != proc->cred.gid && gid != proc->cred.egid &&
            gid != proc->cred.sgid)
        {
            return -EPERM;
        }
        proc->cred.egid = (gid_t)gid;
    }

    return 0;
}

ssize_t sys_setresuid()
{
    struct process *proc = get_current();

    // parameter 0: uid_t ruid
    int32_t ruid;
    argint(0, &ruid);

    // parameter 1: uid_t euid
    int32_t euid;
    argint(1, &euid);

    // parameter 2: uid_t suid
    int32_t suid;
    argint(2, &suid);

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

ssize_t sys_setresgid()
{
    struct process *proc = get_current();

    // parameter 0: gid_t rgid
    int32_t rgid;
    argint(0, &rgid);

    // parameter 1: gid_t egid
    int32_t egid;
    argint(1, &egid);

    // parameter 2: gid_t sgid
    int32_t sgid;
    argint(2, &sgid);

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
