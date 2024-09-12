/* SPDX-License-Identifier: MIT */

#include <arch/fence.h>
#include <fs/xv6fs/log.h>
#include <kernel/elf.h>
#include <kernel/exec.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/memlayout.h>

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int32_t loadseg(pagetable_t pagetable, size_t va, struct inode *ip,
                       size_t offset, size_t sz);

int32_t flags2perm(int32_t flags)
{
    int32_t perm = 0;
    if (flags & 0x1) perm = PTE_X;
    if (flags & 0x2) perm |= PTE_W;
    return perm;
}

int32_t execv(char *path, char **argv)
{
    size_t argc, sz = 0, sp, ustack[MAX_EXEC_ARGS], stackbase;
    struct elfhdr elf;
    struct proghdr ph;
    pagetable_t pagetable = NULL;
    pagetable_t oldpagetable = NULL;

    log_begin_fs_transaction();

    struct inode *ip = inode_from_path(path);
    if (ip == NULL)
    {
        log_end_fs_transaction();
        return -1;
    }
    inode_lock(ip);

    // Check ELF header
    if (inode_read(ip, 0, (size_t)&elf, 0, sizeof(elf)) != sizeof(elf))
        goto bad;

    if (elf.magic != ELF_MAGIC) goto bad;

    struct process *proc = get_current();
    if ((pagetable = proc_pagetable(proc)) == NULL) goto bad;

    // Load program into memory.
    int32_t i = 0;
    int32_t off = elf.phoff;
    for (; i < elf.phnum; i++, off += sizeof(ph))
    {
        if (inode_read(ip, 0, (size_t)&ph, off, sizeof(ph)) != sizeof(ph))
            goto bad;
        if (ph.type != ELF_PROG_LOAD) continue;
        if (ph.memsz < ph.filesz) goto bad;
        if (ph.vaddr + ph.memsz < ph.vaddr) goto bad;
        if (ph.vaddr % PAGE_SIZE != 0) goto bad;
        size_t sz1;
        if ((sz1 = uvm_alloc(pagetable, sz, ph.vaddr + ph.memsz,
                             flags2perm(ph.flags))) == 0)
            goto bad;
        sz = sz1;
        if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0) goto bad;
    }
    inode_unlock_put(ip);
    log_end_fs_transaction();
    ip = NULL;

    // Depending on the cpu implementation a memory barrier might
    // not affect the instruction caches, so after loading executable
    // code an instruction memory barrier is needed.
    // Todo: this should happen on all cores that want to run this process.
    instruction_memory_barrier();

    size_t oldsz = proc->sz;

    // Allocate some pages at the next page boundary:
    // Make the first inaccessible as a stack guard.
    // Use the second (or more) as the user stack.
    sz = PAGE_ROUND_UP(sz);
    size_t alloc_size = (STACK_PAGES_PER_PROCESS + 1) * PAGE_SIZE;
    size_t sz1 = uvm_alloc(pagetable, sz, sz + alloc_size, PTE_W);
    if (sz1 == 0)
    {
        goto bad;
    }
    sz = sz1;
    uvm_clear(pagetable, sz - alloc_size);
    sp = sz;
    stackbase = sp - STACK_PAGES_PER_PROCESS * PAGE_SIZE;

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
    sp -= (argc + 1) * sizeof(size_t);
    sp -= sp % 16;
    if (sp < stackbase) goto bad;
    if (uvm_copy_out(pagetable, sp, (char *)ustack,
                     (argc + 1) * sizeof(size_t)) < 0)
        goto bad;

    // arguments to user main(argc, argv)
    // argc is returned via the system call return
    // value, which goes in a0.
    proc->trapframe->a1 = sp;

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

    // Commit to the user image.
    oldpagetable = proc->pagetable;
    proc->pagetable = pagetable;
    proc->sz = sz;

    trapframe_set_program_counter(proc->trapframe, elf.entry);
    trapframe_set_stack_pointer(proc->trapframe, sp);
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

static int32_t loadseg(pagetable_t pagetable, size_t va, struct inode *ip,
                       size_t offset, size_t sz)
{
    for (size_t i = 0; i < sz; i += PAGE_SIZE)
    {
        size_t pa = uvm_get_physical_addr(pagetable, va + i);
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

        if (inode_read(ip, 0, pa, offset + i, n) != n)
        {
            return -1;
        }
    }

    return 0;
}
