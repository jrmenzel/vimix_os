/* SPDX-License-Identifier: MIT */

#include <fs/xv6fs/log.h>
#include <kernel/elf.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/vm.h>
#include <mm/memlayout.h>

static int loadseg(pde_t *, uint64, struct inode *, uint, uint);

int flags2perm(int flags)
{
    int perm = 0;
    if (flags & 0x1) perm = PTE_X;
    if (flags & 0x2) perm |= PTE_W;
    return perm;
}

int execv(char *path, char **argv)
{
    char *s, *last;
    int i, off;
    uint64 argc, sz = 0, sp, ustack[MAX_EXEC_ARGS], stackbase;
    struct elfhdr elf;
    struct inode *ip;
    struct proghdr ph;
    pagetable_t pagetable = 0, oldpagetable;
    struct process *proc = get_current();

    log_begin_fs_transaction();

    if ((ip = inode_from_path(path)) == 0)
    {
        log_end_fs_transaction();
        return -1;
    }
    inode_lock(ip);

    // Check ELF header
    if (inode_read(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
        goto bad;

    if (elf.magic != ELF_MAGIC) goto bad;

    if ((pagetable = proc_pagetable(proc)) == 0) goto bad;

    // Load program into memory.
    for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph))
    {
        if (inode_read(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
            goto bad;
        if (ph.type != ELF_PROG_LOAD) continue;
        if (ph.memsz < ph.filesz) goto bad;
        if (ph.vaddr + ph.memsz < ph.vaddr) goto bad;
        if (ph.vaddr % PAGE_SIZE != 0) goto bad;
        uint64 sz1;
        if ((sz1 = uvm_alloc(pagetable, sz, ph.vaddr + ph.memsz,
                             flags2perm(ph.flags))) == 0)
            goto bad;
        sz = sz1;
        if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0) goto bad;
    }
    inode_unlock_put(ip);
    log_end_fs_transaction();
    ip = 0;

    proc = get_current();
    uint64 oldsz = proc->sz;

    // Allocate two pages at the next page boundary.
    // Make the first inaccessible as a stack guard.
    // Use the second as the user stack.
    sz = PAGE_ROUND_UP(sz);
    uint64 sz1;
    if ((sz1 = uvm_alloc(pagetable, sz, sz + 2 * PAGE_SIZE, PTE_W)) == 0)
        goto bad;
    sz = sz1;
    uvm_clear(pagetable, sz - 2 * PAGE_SIZE);
    sp = sz;
    stackbase = sp - PAGE_SIZE;

    // Push argument strings, prepare rest of stack in ustack.
    for (argc = 0; argv[argc]; argc++)
    {
        if (argc >= MAX_EXEC_ARGS) goto bad;
        sp -= strlen(argv[argc]) + 1;
        sp -= sp % 16;  // riscv sp must be 16-byte aligned
        if (sp < stackbase) goto bad;
        if (uvm_copy_out(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
            goto bad;
        ustack[argc] = sp;
    }
    ustack[argc] = 0;

    // push the array of argv[] pointers.
    sp -= (argc + 1) * sizeof(uint64);
    sp -= sp % 16;
    if (sp < stackbase) goto bad;
    if (uvm_copy_out(pagetable, sp, (char *)ustack,
                     (argc + 1) * sizeof(uint64)) < 0)
        goto bad;

    // arguments to user main(argc, argv)
    // argc is returned via the system call return
    // value, which goes in a0.
    proc->trapframe->a1 = sp;

    // Save program name for debugging.
    for (last = s = path; *s; s++)
        if (*s == '/') last = s + 1;
    safestrcpy(proc->name, last, sizeof(proc->name));

    // Commit to the user image.
    oldpagetable = proc->pagetable;
    proc->pagetable = pagetable;
    proc->sz = sz;
    proc->trapframe->epc = elf.entry;  // initial program counter = main
    proc->trapframe->sp = sp;          // initial stack pointer
    proc_free_pagetable(oldpagetable, oldsz);

    return argc;  // this ends up in a0, the first argument to main(argc, argv)

bad:
    if (pagetable) proc_free_pagetable(pagetable, sz);
    if (ip)
    {
        inode_unlock_put(ip);
        log_end_fs_transaction();
    }
    return -1;
}

/// Load a program segment into pagetable at virtual address va.
/// va must be page-aligned
/// and the pages from va to va+sz must already be mapped.
/// Returns 0 on success, -1 on failure.
static int loadseg(pagetable_t pagetable, uint64 va, struct inode *ip,
                   uint offset, uint sz)
{
    uint i, n;
    uint64 pa;

    for (i = 0; i < sz; i += PAGE_SIZE)
    {
        pa = uvm_get_physical_addr(pagetable, va + i);
        if (pa == 0) panic("loadseg: address should exist");
        if (sz - i < PAGE_SIZE)
            n = sz - i;
        else
            n = PAGE_SIZE;
        if (inode_read(ip, 0, (uint64)pa, offset + i, n) != n) return -1;
    }

    return 0;
}
