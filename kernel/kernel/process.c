/* SPDX-License-Identifier: MIT */

#include <arch/trapframe.h>
#include <kernel/ipi.h>
#include <kernel/kernel.h>
#include <kernel/pgtable.h>
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

    // A user page table with kernel stack, trapframe, etc. but no program code
    // or data yet.
    proc->pagetable = proc_pagetable(proc, true);
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
    proc->umask = 0;

    // Set up new context to start executing at forkret,
    // which returns to user space.
    // proc was zero initialized, so is proc->context at this point
    context_set_return_register(&proc->context, (size_t)(forkret));
    context_set_stack_pointer(&proc->context, proc->kstack + KERNEL_STACK_SIZE);

    DEBUG_ASSERT_CPU_HOLDS_LOCK(&proc->lock);

    return proc;
}

/// free a struct process structure and the data hanging from it,
/// including user pages.
void process_free(struct process *proc)
{
    if (proc->trapframe)
    {
        free_page((void *)proc->trapframe);
    }
    proc->trapframe = NULL;

    if (proc->pagetable)
    {
        page_table_free(proc->pagetable);
    }
    proc->pagetable = NULL;

    // unmap and free kernel stack:
    if (proc->kstack != 0)
    {
        spin_lock(&g_kernel_pagetable->lock);
        // remove from memory map
        page_table_unmap_remove_range(g_kernel_pagetable, proc->kstack,
                                      KERNEL_STACK_PAGES * PAGE_SIZE);

        vm_trim_pagetable(g_kernel_pagetable, proc->kstack);
        // update pagetable, flush cache:
        page_table_apply_mapping(g_kernel_pagetable);
        spin_unlock(&g_kernel_pagetable->lock);

        // now allow the re-use of the kernel stack address
        proc_free_kernel_stack(proc->kstack);

        proc->kstack = 0;
    }

    if (proc->cred.groups != NULL)
    {
        put_group_info(proc->cred.groups);
        proc->cred.groups = NULL;
    }

    if (proc->lock.locked)
    {
        spin_unlock(&proc->lock);
    }

    if (proc->xdbg_info)
    {
        debug_info_free(proc->xdbg_info);
        proc->xdbg_info = NULL;
    }

    kfree(proc);
}

bool proc_init_kernel_stack(struct Page_Table *kpage_table,
                            struct process *proc,
                            struct Page_Table *proc_pagetable)
{
    // init per process kernel stack, so modify kernels page table

    bool failure = false;
    spin_lock(&kpage_table->lock);
    for (size_t i = 0; i < KERNEL_STACK_PAGES; ++i)
    {
        char *page_va = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
        if (page_va == NULL)
        {
            failure = true;
            break;
        }
        size_t page_pa = virt_to_phys((size_t)page_va);

        struct MM_Region *region =
            mm_region_alloc_init(page_pa, proc->kstack + (i * PAGE_SIZE),
                                 PAGE_SIZE, MM_REGION_USER_KSTACK);
        if (region == NULL)
        {
            free_page(page_va);
            failure = true;
            break;
        }
        memory_map_add_single_region(&kpage_table->memory_map, region);

#if defined(MAP_KERNEL_STACK_TO_USER_PT)
        struct MM_Region *region_user =
            mm_region_alloc_init(page_pa, proc->kstack + (i * PAGE_SIZE),
                                 PAGE_SIZE, MM_REGION_USER_KSTACK_MAP);
        if (region_user == NULL)
        {
            failure = true;
            break;
        }

        memory_map_add_single_region(&proc_pagetable->memory_map, region_user);
#endif
    }

    if (failure)
    {
        page_table_unmap_partial_mappings(kpage_table);
        spin_unlock(&kpage_table->lock);
        return false;
    }

#if defined(MAP_KERNEL_STACK_TO_USER_PT)
    // Update new page table first, if applying the kernel page below fails,
    // the user page table gets deleted anyways. This makes
    // roll back of the kernel page table in case of failure easier.
    if (page_table_apply_mapping(proc_pagetable) < 0)
    {
        page_table_unmap_partial_mappings(kpage_table);
        spin_unlock(&kpage_table->lock);
        return false;
    }
#endif
    syserr_t err = kvm_apply_kernel_mapping(kpage_table);
    spin_unlock(&kpage_table->lock);

    if (err < 0)
    {
        return false;
    }

    // tell other cores also to reload the kernel page table (after we unlocked
    // it)
    cpu_mask mask = ipi_cpu_mask_all_but_self();

    // test skips the ipi call when no other CPUs are booted
    if (mask != 0)
    {
        ipi_send_interrupt(mask, IPI_KERNEL_PAGETABLE_CHANGED, NULL);
    }

    return true;
}

void proc_free_kernel_stack(size_t stack_va)
{
    spin_lock(&g_process_list.kernel_stack_lock);
    size_t idx = KSTACK_INDEX_FROM_VA(stack_va);
    clear_bit(g_process_list.kernel_stack_in_use, idx);
    spin_unlock(&g_process_list.kernel_stack_lock);
}
