/* SPDX-License-Identifier: MIT */

#include <arch/context.h>
#include <arch/trapframe.h>
#include <fs/fs_lookup.h>
#include <kernel/elf.h>
#include <kernel/errno.h>
#include <kernel/exec.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/permission.h>
#include <kernel/pgtable.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/memory_map.h>
#include <mm/vm.h>

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int32_t loadseg(struct Page_Table *pagetable, size_t va,
                       struct inode *ip, size_t offset, size_t sz);

/// @brief Converts access flags from an ELF file to VM access permissions.
/// @param elf_flags Flags from an ELF file
/// @param flags_in pte flags in to modify
/// @return A pte with matching access flags
pte_t elf_flags_to_perm(int32_t elf_flags, pte_t flags_in)
{
    flags_in = (elf_flags & 0x1) ? pte_set_executable(flags_in)
                                 : pte_unset_executable(flags_in);
    flags_in = (elf_flags & 0x2) ? pte_set_writeable(flags_in)
                                 : pte_unset_writeable(flags_in);
    return flags_in;
}

enum MM_Region_Type elf_flags_to_map_type(int32_t elf_flags)
{
    bool is_executable = elf_flags & 0x1;
    bool is_writeable = elf_flags & 0x2;

    if (is_executable && is_writeable)
    {
        return MM_REGION_USER_RW_TEXT;
    }
    else if (is_executable && !is_writeable)
    {
        return MM_REGION_USER_TEXT;
    }
    else if (!is_executable && is_writeable)
    {
        return MM_REGION_USER_DATA;
    }
    else
    {
        return MM_REGION_USER_RO_DATA;
    }
}

bool load_program_to_memory(struct inode *ip, struct elfhdr *elf,
                            struct Page_Table *pagetable, size_t *last_va)
{
    for (int32_t i = 0; i < elf->phnum; i++)
    {
        // read program header i
        struct proghdr ph;
        int32_t off = elf->phoff + i * sizeof(struct proghdr);
        if (VFS_INODE_READ_KERNEL(ip, off, (size_t)&ph, sizeof(ph)) !=
            sizeof(ph))
        {
            return false;
        }

        // ignore if not intended to be loaded
        if (ph.type != ELF_PROG_LOAD) continue;

        // error checks
        if ((ph.memsz < ph.filesz) || (ph.vaddr + ph.memsz < ph.vaddr) ||
            (ph.vaddr % PAGE_SIZE != 0))
        {
            return false;
        }

        // allocate pages and update last_va
        size_t alloc_size = (ph.vaddr + ph.memsz) - *last_va;

        size_t sz = uvm_alloc_heap(pagetable, *last_va, alloc_size,
                                   elf_flags_to_map_type(ph.flags));
        if (sz != alloc_size)
        {
            return false;
        }
        *last_va += alloc_size;

        // load actual data
        if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
        {
            return false;
        }
    }
    return true;
}

syserr_t do_execv(char *path, char **argv)
{
    syserr_t error = 0;
    struct dentry *dp = dentry_from_path(path, &error);
    if (dp == NULL)
    {
        return error;
    }
    if (dentry_is_invalid(dp))
    {
        dentry_put(dp);
        return -ENOENT;
    }

    struct process *proc = get_current();
    syserr_t perm = check_dentry_permission(proc, dp, MAY_EXEC);
    if (perm < 0)
    {
        dentry_put(dp);
        return perm;
    }

    struct inode *ip = inode_get(dp->ip);
    dentry_put(dp);
    inode_lock(ip);

    // grab setuid/setgid bits while ip is locked
    bool set_uid = S_ISUID & ip->i_mode;
    bool set_gid = S_ISGID & ip->i_mode;
    uid_t new_uid = ip->uid;
    gid_t new_gid = ip->gid;

    // Check ELF header
    struct elfhdr elf;
    size_t header_read =
        VFS_INODE_READ_KERNEL(ip, 0, (size_t)&elf, sizeof(elf));
    if (header_read != sizeof(elf) || elf.magic != ELF_MAGIC)
    {
        inode_unlock_put(ip);
        return -ENOEXEC;
    }

    struct Page_Table *pagetable = proc_pagetable(proc);
    if (pagetable == NULL)
    {
        inode_unlock_put(ip);
        return -ENOMEM;
    }

    // Load program into memory.
    size_t heap_begin = USER_TEXT_START;  // last virtual address used for
                                          // loading binary and data
    bool fatal_error =
        !load_program_to_memory(ip, &elf, pagetable, &heap_begin);
    inode_unlock_put(ip);
    ip = NULL;

    // check error after handling the inode as this would have to be done now
    // anyways
    if (fatal_error)
    {
        page_table_free(pagetable);
        return -ENOMEM;
    }

    size_t stack_low;
    size_t sp;
    size_t argc = uvm_create_stack(pagetable, argv, &stack_low, &sp);
    if (argc == -1)
    {
        page_table_free(pagetable);
        return -ENOMEM;
    }

    // Clear all registers to not spill information from the caller of execv
    // also the stack frame and return address will be set to 0 which helps
    // detect invalid uses and the end of the stack frames.
    memset(proc->trapframe, 0, sizeof(struct trapframe));

    // arguments to user main(argc, argv)
    // argc is returned via the system call return
    // value, which goes in a0.
    trapframe_set_argument_register(proc->trapframe, 1, sp);

    // Save program name for debugging.
    char *s = path;
    char *last = path;
    for (; *s; s++)
    {
        if (*s == '/')
        {
            last = s + 1;
        }
    }
    safestrcpy(proc->name, last, sizeof(proc->name));

    // Align the heap to begin at a 16 byte boundry
    // An unaligned heap can lead to unaligned acceses
    // after sbrk/malloc. No change if already aligned.
    heap_begin += (16 - heap_begin % 16) % 16;

    // Commit to the user image.
    struct Page_Table *oldpagetable = proc->pagetable;
    spin_lock(&oldpagetable->lock);

    proc->pagetable = pagetable;
    proc->heap_begin = heap_begin;
    proc->heap_end = proc->heap_begin;
    proc->stack_low = stack_low;

    trapframe_set_program_counter(proc->trapframe, elf.entry);
    trapframe_set_stack_pointer(proc->trapframe, sp);
    page_table_free(oldpagetable);

    // change UID/GID after anything that could fail
    if (set_uid)
    {
        proc->cred.euid = new_uid;
        proc->cred.suid = new_uid;
    }
    if (set_gid)
    {
        proc->cred.egid = new_gid;
        proc->cred.sgid = new_gid;
    }

    // This ends up in a0, the first argument to main(argc, argv)
    // not a return value of the syscall! It does not return!
    return argc;
}

static int32_t loadseg(struct Page_Table *pagetable, size_t va,
                       struct inode *ip, size_t offset, size_t sz)
{
    for (size_t i = 0; i < sz; i += PAGE_SIZE)
    {
        spin_lock(&pagetable->lock);
        size_t pa = uvm_get_physical_paddr(pagetable, va + i, NULL);
        spin_unlock(&pagetable->lock);
        if (pa == 0)
        {
            panic("loadseg: address should exist");
        }

        size_t n;
        if (sz - i < PAGE_SIZE)
        {
            n = sz - i;
        }
        else
        {
            n = PAGE_SIZE;
        }

        size_t page_kva = phys_to_virt(pa);
        if (VFS_INODE_READ_KERNEL(ip, offset + i, page_kva, n) != n)
        {
            return -1;
        }
    }

    return 0;
}
