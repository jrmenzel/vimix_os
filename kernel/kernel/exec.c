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
    log_begin_fs_transaction();

    struct inode *ip = inode_from_path(path);
    if (ip == NULL)
    {
        log_end_fs_transaction();
        return -1;
    }
    inode_lock(ip);

    // Check ELF header
    struct elfhdr elf;
    size_t header_read = inode_read(ip, false, (size_t)&elf, 0, sizeof(elf));
    if (header_read != sizeof(elf) || elf.magic != ELF_MAGIC)
    {
        inode_unlock_put(ip);
        log_end_fs_transaction();
        return -1;
    }

    struct process *proc = get_current();
    pagetable_t pagetable = proc_pagetable(proc);
    if (pagetable == NULL)
    {
        inode_unlock_put(ip);
        log_end_fs_transaction();
        return -1;
    }

    // process size
    size_t sz = 0;

    // Load program into memory.
    bool fatal_error = false;
    for (int32_t i = 0; i < elf.phnum; i++)
    {
        // read program header i
        struct proghdr ph;
        int32_t off = elf.phoff + i * sizeof(struct proghdr);
        if (inode_read(ip, false, (size_t)&ph, off, sizeof(ph)) != sizeof(ph))
        {
            fatal_error = true;
            break;
        }

        // ignore if not intended to be loaded
        if (ph.type != ELF_PROG_LOAD) continue;

        // error checks
        if ((ph.memsz < ph.filesz) || (ph.vaddr + ph.memsz < ph.vaddr) ||
            (ph.vaddr % PAGE_SIZE != 0))
        {
            fatal_error = true;
            break;
        }

        // allocate pages and update sz
        size_t sz1 =
            uvm_alloc(pagetable, sz, ph.vaddr + ph.memsz - USER_TEXT_START,
                      flags2perm(ph.flags));
        if (sz1 == 0)
        {
            fatal_error = true;
            break;
        }
        sz = sz1;

        // load actual data
        if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
        {
            fatal_error = true;
            break;
        }
    }
    inode_unlock_put(ip);
    log_end_fs_transaction();
    ip = NULL;

    // check error after handling the inode as this would have to be done now
    // anyways
    if (fatal_error)
    {
        proc_free_pagetable(pagetable, sz);
        return -1;
    }

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
        proc_free_pagetable(pagetable, sz);
        return -1;
    }
    sz = sz1;

    size_t sp = sz + USER_TEXT_START;
    uvm_clear_user_access_bit(pagetable, sp - alloc_size);
    size_t stackbase = sp - STACK_PAGES_PER_PROCESS * PAGE_SIZE;

    // Push argument strings, prepare rest of stack in ustack.
    size_t argc, ustack[MAX_EXEC_ARGS];
    for (argc = 0; argv[argc] && argc < MAX_EXEC_ARGS; argc++)
    {
        // 16-byte aligned space on the stack for the string
        // (sp alignement is a RISC V requirement)
        sp -= strlen(argv[argc]) + 1;
        sp -= sp % 16;
        if (sp < stackbase)
        {
            // stack overflow
            fatal_error = true;
            break;
        }

        if (uvm_copy_out(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
        {
            fatal_error = true;
            break;
        }
        ustack[argc] = sp;
    }
    if (fatal_error || argc >= MAX_EXEC_ARGS)
    {
        proc_free_pagetable(pagetable, sz);
        return -1;
    }
    ustack[argc] = 0;

    // push the array of argv[] pointers.
    sp -= (argc + 1) * sizeof(size_t);
    sp -= sp % 16;
    if (sp < stackbase)
    {
        proc_free_pagetable(pagetable, sz);
        return -1;
    }
    if (uvm_copy_out(pagetable, sp, (char *)ustack,
                     (argc + 1) * sizeof(size_t)) < 0)
    {
        proc_free_pagetable(pagetable, sz);
        return -1;
    }

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
    pagetable_t oldpagetable = proc->pagetable;
    proc->pagetable = pagetable;
    proc->sz = sz;

    trapframe_set_program_counter(proc->trapframe, elf.entry);
    trapframe_set_stack_pointer(proc->trapframe, sp);
    proc_free_pagetable(oldpagetable, oldsz);

    return argc;  // this ends up in a0, the first argument to main(argc, argv)
}

static int32_t loadseg(pagetable_t pagetable, size_t va, struct inode *ip,
                       size_t offset, size_t sz)
{
    for (size_t i = 0; i < sz; i += PAGE_SIZE)
    {
        size_t pa = uvm_get_physical_addr(pagetable, va + i, NULL);
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

        if (inode_read(ip, false, pa, offset + i, n) != n)
        {
            return -1;
        }
    }

    return 0;
}
