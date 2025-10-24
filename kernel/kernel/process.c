/* SPDX-License-Identifier: MIT */

#include <arch/trapframe.h>
#include <kernel/ipi.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/process.h>
#include <kernel/string.h>
#include <mm/kalloc.h>
#include <mm/memlayout.h>

atomic_int32_t g_next_pid = 1;

struct group_info *groups_alloc(size_t ngroups)
{
    struct group_info *gi = (struct group_info *)kmalloc(
        sizeof(struct group_info), ALLOC_FLAG_NONE);
    if (gi == NULL)
    {
        return NULL;
    }

    gi->gid = (gid_t *)kmalloc(ngroups * sizeof(gid_t), ALLOC_FLAG_ZERO_MEMORY);
    if (gi->gid == NULL)
    {
        kfree(gi);
        return NULL;
    }

    kref_init(&gi->usage);
    gi->ngroups = ngroups;
    return gi;
}

void groups_free(struct group_info *gi)
{
    kfree(gi->gid);
    kfree(gi);
}

void proc_free_kobject(struct kobject *kobj)
{
    if (kobj == NULL) return;

    struct process *proc = process_from_kobj(kobj);
    process_free(proc);
}
struct kobj_type proc_ktype = {.release = proc_free_kobject,
                               .sysfs_ops = NULL,
                               .attribute = NULL,
                               .n_attributes = 0};

/// Get a new unique process ID
static inline pid_t alloc_pid()
{
    return (pid_t)atomic_fetch_add(&g_next_pid, 1);
}

/// Creates a new process:
/// If allocated, initialize state required to run in the kernel,
/// and return with `proc->lock` held.
/// If there are no free processes, or a memory allocation fails, return NULL.
struct process *process_alloc_init()
{
    struct process *proc =
        kmalloc(sizeof(struct process), ALLOC_FLAG_ZERO_MEMORY);
    if (proc == NULL)
    {
        return NULL;
    }
    // process_free() (called from last proc_put()) can free partially
    // initialized structs, but the lock is expected to be helt
    kobject_init(&proc->kobj, &proc_ktype);
    spin_lock_init(&proc->lock, "proc");
    spin_lock(&proc->lock);

    // kernel stack
    proc->kstack = proc_get_kernel_stack();
    if (proc->kstack == 0)
    {
        proc_put(proc);
        return NULL;
    }

    bool pagetable_updated =
        proc_init_kernel_stack(g_kernel_pagetable, proc, proc->kstack);
    if (pagetable_updated == false)
    {
        // a bit special case: free_proc() expects the kernel stack to be set in
        // the pagetable if proc->kstack != 0is set. So free the kernel stack
        // part that is not the pagetable manually here. proc_put() will call
        // free_proc().
        proc_free_kernel_stack(proc->kstack);
        proc->kstack = 0;
        proc_put(proc);
        return NULL;
    }

    // Allocate a trapframe page (full page as it gets it's own memory mapping
    // to a compile time known location).
    _Static_assert(sizeof(struct trapframe) <= PAGE_SIZE,
                   "struct trapframe is too big");
    proc->trapframe = (struct trapframe *)alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    if (proc->trapframe == NULL)
    {
        proc_put(proc);
        return NULL;
    }

    // An empty user page table.
    proc->pagetable = proc_pagetable(proc);
    if (proc->pagetable == NULL)
    {
        proc_put(proc);
        return NULL;
    }

    // other members and state
    list_init(&proc->plist);
    proc->pid = alloc_pid();
    proc->state = USED;

    // all user and group IDs to 0
    memset(&proc->cred, 0, sizeof(struct cred));
    proc->cred.groups = NULL;

    // printk(
    //     "CPU %zd: new  process %d at kstack: 0x%zx (lower address) trapframe:
    //     " "0x%zx\n",
    //     __arch_smp_processor_id(), proc->pid, proc->kstack,
    //     (size_t)proc->trapframe);

    // Set up new context to start executing at forkret,
    // which returns to user space.
    // proc was zero initialized, so is proc->context at this point
    context_set_return_register(&proc->context, (xlen_t)forkret);
    context_set_stack_pointer(&proc->context, proc->kstack + KERNEL_STACK_SIZE);

    DEBUG_ASSERT_CPU_HOLDS_LOCK(&proc->lock);
    return proc;
}

/// free a struct process structure and the data hanging from it,
/// including user pages.
/// proc->lock must be held.
void process_free(struct process *proc)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(&proc->lock);

    if (proc->trapframe)
    {
        free_page((void *)proc->trapframe);
    }
    proc->trapframe = NULL;

    if (proc->pagetable)
    {
        proc_free_pagetable(proc->pagetable);
    }
    proc->pagetable = INVALID_PAGETABLE_T;

    // unmap and free kernel stack:
    if (proc->kstack != 0)
    {
        proc_free_kernel_stack(proc->kstack);
        spin_lock(&g_kernel_pagetable_lock);
        uvm_unmap(g_kernel_pagetable, proc->kstack, KERNEL_STACK_PAGES, true);
        vm_trim_pagetable(g_kernel_pagetable, proc->kstack);
        mmu_set_page_table((size_t)g_kernel_pagetable,
                           0);  // update pagetable, flush cache
        spin_unlock(&g_kernel_pagetable_lock);

        // tell other cores to also to reload the kernel page table
        cpu_mask mask = ipi_cpu_mask_all_but_self();
        ipi_send_interrupt(mask, IPI_KERNEL_PAGETABLE_CHANGED, NULL);

        proc->kstack = 0;
    }

    if (proc->cred.groups != NULL)
    {
        put_group_info(proc->cred.groups);
        proc->cred.groups = NULL;
    }

    spin_unlock(&proc->lock);
    kfree(proc);
}

bool proc_init_kernel_stack(pagetable_t kpage_table, struct process *proc,
                            size_t kstack_va)
{
    spin_lock(&g_kernel_pagetable_lock);
    for (size_t i = 0; i < KERNEL_STACK_PAGES; ++i)
    {
        char *pa = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
        if (pa == NULL)
        {
            spin_unlock(&g_kernel_pagetable_lock);
            return false;
        }
        kvm_map_or_panic(kpage_table, kstack_va + (i * PAGE_SIZE), (size_t)pa,
                         PAGE_SIZE, PTE_KERNEL_STACK);
    }
    mmu_set_page_table((size_t)kpage_table,
                       0);  // update pagetable, flush cache

    spin_unlock(&g_kernel_pagetable_lock);

    // tell other cores to also to reload the kernel page table
    cpu_mask mask = ipi_cpu_mask_all_but_self();
    ipi_send_interrupt(mask, IPI_KERNEL_PAGETABLE_CHANGED, NULL);

    return true;
}

void proc_free_kernel_stack(size_t stack_va)
{
    spin_lock(&g_process_list.kernel_stack_lock);
    size_t idx = KSTACK_INDEX_FROM_VA(stack_va);
    clear_bit(idx, g_process_list.kernel_stack_in_use);
    spin_unlock(&g_process_list.kernel_stack_lock);
}
