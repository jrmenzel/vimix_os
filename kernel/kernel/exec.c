/* SPDX-License-Identifier: MIT */

#include <arch/context.h>
#include <arch/fence.h>
#include <fs/xv6fs/log.h>
#include <kernel/elf.h>
#include <kernel/exec.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/vm.h>

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int32_t loadseg(pagetable_t pagetable, size_t va, struct inode *ip,
                       size_t offset, size_t sz);

bool load_program_to_memory(struct inode *ip, struct elfhdr *elf,
                            pagetable_t pagetable, size_t *last_va)
{
    for (int32_t i = 0; i < elf->phnum; i++)
    {
        // read program header i
        struct proghdr ph;
        int32_t off = elf->phoff + i * sizeof(struct proghdr);
        if (inode_read(ip, false, (size_t)&ph, off, sizeof(ph)) != sizeof(ph))
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
                                   elf_flags_to_perm(ph.flags));
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

    // Load program into memory.
    size_t heap_begin = USER_TEXT_START;  // last virtual address used for
                                          // loading binary and data
    bool fatal_error =
        !load_program_to_memory(ip, &elf, pagetable, &heap_begin);
    inode_unlock_put(ip);
    log_end_fs_transaction();
    ip = NULL;

    // check error after handling the inode as this would have to be done now
    // anyways
    if (fatal_error)
    {
        proc_free_pagetable(pagetable);
        return -1;
    }

    // Depending on the cpu implementation a memory barrier might
    // not affect the instruction caches, so after loading executable
    // code an instruction memory barrier is needed.
    // Todo: this should happen on all cores that want to run this process.
    instruction_memory_barrier();

    size_t stack_low;
    size_t sp;
    size_t argc = uvm_create_stack(pagetable, argv, &stack_low, &sp);
    if (argc == -1)
    {
        proc_free_pagetable(pagetable);
        return -1;
    }

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
    pagetable_t oldpagetable = proc->pagetable;
    proc->pagetable = pagetable;
    proc->heap_begin = heap_begin;
    proc->heap_end = proc->heap_begin;
    proc->stack_low = stack_low;

    trapframe_set_program_counter(proc->trapframe, elf.entry);
    trapframe_set_stack_pointer(proc->trapframe, sp);
    proc_free_pagetable(oldpagetable);

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
