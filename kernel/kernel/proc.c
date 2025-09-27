/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/trap.h>
#include <arch/trapframe.h>
#include <fs/xv6fs/xv6fs.h>
#include <kernel/cpu.h>
#include <kernel/errno.h>
#include <kernel/exec.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/ipi.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/signal.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/kalloc.h>
#include <mm/memlayout.h>
#include <mm/vm.h>
#include <syscalls/syscall.h>

struct cpu g_cpus[MAX_CPUS] = {0};
struct spinlock g_cpus_ipi_lock;

/// @brief All user processes
struct process_list g_process_list;

/// @brief The init process in user mode.
/// Created in userspace_init(), the only process not created by fork()
struct process *g_initial_user_process;

atomic_int32_t g_next_pid = 1;

extern void forkret();
void wakeup_holding_plist_lock(void *chan);

extern char trampoline[];  // u_mode_trap_vector.S

/// helps ensure that wakeups of wait()ing
/// parents are not lost. helps obey the
/// memory model when using p->parent.
/// must be acquired before any p->lock.
struct spinlock g_wait_lock;

/// @brief Returns the virtual address for a kernel stack.
/// @return 0 on failure
size_t proc_get_kernel_stack()
{
    spin_lock(&g_process_list.kernel_stack_lock);
    ssize_t idx =
        find_first_zero_bit(g_process_list.kernel_stack_in_use, MAX_PROCESSES);
    if (idx < 0)
    {
        spin_unlock(&g_process_list.kernel_stack_lock);
        return 0;
    }
    set_bit(idx, g_process_list.kernel_stack_in_use);
    spin_unlock(&g_process_list.kernel_stack_lock);
    // printk("alloc kstack va 0x%zd\n", KSTACK_VA_FROM_INDEX(idx));
    return KSTACK_VA_FROM_INDEX(idx);
}

void proc_free_kernel_stack(size_t stack_va)
{
    spin_lock(&g_process_list.kernel_stack_lock);
    // printk("free kstack va 0x%zd\n", stack_va);
    size_t idx = KSTACK_INDEX_FROM_VA(stack_va);
    clear_bit(idx, g_process_list.kernel_stack_in_use);
    spin_unlock(&g_process_list.kernel_stack_lock);
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

/// initialize the g_process_array table.
void proc_init()
{
    spin_lock_init(&g_wait_lock, "wait_lock");

    list_init(&g_process_list.plist);
    rwspin_lock_init(&g_process_list.lock, "proc_list_lock");
    g_process_list.kernel_stack_in_use = bitmap_alloc(MAX_PROCESSES);
    spin_lock_init(&g_process_list.kernel_stack_lock, "proc_list_kstack_lock");
}

/// Return this CPU's cpu struct.
/// Interrupts must be disabled as long as the
/// returned struct is used (as a context switch
/// invalidates the cpu if the kernel process switched cores)
__attribute__((returns_nonnull)) struct cpu *get_cpu()
{
#if defined(CONFIG_DEBUG_EXTRA_RUNTIME_TESTS)
    if (cpu_is_interrupts_enabled())
    {
        panic("interrupts must be disabled when calling get_cpu");
    }
#endif  // CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
    size_t id = smp_processor_id();
    struct cpu *c = &g_cpus[id];
    return c;
}

struct process *get_current()
{
    cpu_push_disable_device_interrupt_stack();
    struct cpu *c = get_cpu();
    struct process *proc = c->proc;
    cpu_pop_disable_device_interrupt_stack();
    return proc;
}

/// Get a new unique process ID
pid_t alloc_pid() { return (pid_t)atomic_fetch_add(&g_next_pid, 1); }

void proc_free_kobject(struct kobject *kobj)
{
    if (kobj == NULL) return;

    struct process *proc = process_from_kobj(kobj);
    proc_free(proc);
}
struct kobj_type proc_ktype = {.release = proc_free_kobject,
                               .sysfs_ops = NULL,
                               .attribute = NULL,
                               .n_attributes = 0};

/// Creates a new process:
/// If allocated, initialize state required to run in the kernel,
/// and return with `proc->lock` held.
/// If there are no free processes, or a memory allocation fails, return NULL.
static struct process *alloc_process()
{
    struct process *proc = kmalloc(sizeof(struct process));
    if (proc == NULL)
    {
        return NULL;
    }
    memset(proc, 0, sizeof(struct process));
    // proc_free() (called from last proc_put()) can free partially initialized
    // structs, but the lock is expected to be helt
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
void proc_free(struct process *proc)
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

    spin_unlock(&proc->lock);
    kfree(proc);
}

/// Create a user page table for a given process, with no user memory,
/// but with trampoline and trapframe pages.
pagetable_t proc_pagetable(struct process *proc)
{
    // An empty page table.
    pagetable_t pagetable = (pagetable_t)alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    if (pagetable == INVALID_PAGETABLE_T)
    {
        return INVALID_PAGETABLE_T;
    }

    // map the trampoline code (for system call return)
    // at the highest user virtual address.
    // only the supervisor uses it, on the way
    // to/from user space, so not PTE_U.
    if (vm_map(pagetable, TRAMPOLINE, (size_t)trampoline, PAGE_SIZE,
               PTE_RO_TEXT, false) < 0)
    {
        uvm_free_pagetable(pagetable);
        return INVALID_PAGETABLE_T;
    }

    // map the trapframe page just below the trampoline page, for
    // u_mode_trap_vector.S.
    if (vm_map(pagetable, TRAPFRAME, (size_t)(proc->trapframe), PAGE_SIZE,
               PTE_RW_RAM, false) < 0)
    {
        uvm_unmap(pagetable, TRAMPOLINE, 1, 0);
        uvm_free_pagetable(pagetable);
        return INVALID_PAGETABLE_T;
    }

    return pagetable;
}

void proc_free_pagetable(pagetable_t pagetable)
{
    // unmap pages not owned by this process
    uvm_unmap(pagetable, TRAMPOLINE, 1, false);
    uvm_unmap(pagetable, TRAPFRAME, 1, false);

    // everything left mapped is owned by the process, free everything
    uvm_free_pagetable(pagetable);
}

/// Set up first user process.
/// This creates the only process not created by fork()
void userspace_init()
{
    g_initial_user_process = alloc_process();
    if (g_initial_user_process == NULL)
        panic("userspace_init() already out of memory");
    g_initial_user_process->state = RUNNABLE;

    // add to kobject tree
    kobject_add(&g_initial_user_process->kobj, &g_kobjects_proc, "1");
    kobject_put(&g_initial_user_process->kobj);  // drop reference now
                                                 // that the kobject tree
                                                 // holds one

    // add to process list
    rwspin_write_lock(&g_process_list.lock);
    list_add_tail(&g_initial_user_process->plist, &g_process_list.plist);
    rwspin_write_unlock(&g_process_list.lock);

    spin_unlock(&g_initial_user_process->lock);
}

/// Grow or shrink user memory by n bytes.
/// Return 0 on success, -1 on failure.
int32_t proc_grow_memory(ssize_t n)
{
    struct process *proc = get_current();

    if (n > 0)
    {
        // grow
        if (uvm_alloc_heap(proc->pagetable, proc->heap_end, n, PTE_USER_RAM) !=
            n)
        {
            return -1;
        }
        proc->heap_end += n;
    }
    else if (n < 0)
    {
        // shrink
        n *= -1;
        size_t proc_size = proc->heap_end - proc->heap_begin;
        if (n > proc_size)
        {
            return -1;
        }
        size_t dealloc = uvm_dealloc_heap(proc->pagetable, proc->heap_end, n);
        proc->heap_end -= dealloc;
    }

    return 0;
}

int32_t proc_copy_memory(struct process *src, struct process *dst)
{
    // Copy app code and heap
    if (uvm_copy(src->pagetable, dst->pagetable, USER_TEXT_START,
                 src->heap_end) < 0)
    {
        return -1;
    }
    dst->heap_begin = src->heap_begin;
    dst->heap_end = src->heap_end;

    // Copy user stack
    if (uvm_copy(src->pagetable, dst->pagetable, src->stack_low,
                 USER_STACK_HIGH - 1) < 0)
    {
        return -1;
    }
    dst->stack_low = src->stack_low;

    return 0;
}

ssize_t fork()
{
    // Allocate new process.
    struct process *np = alloc_process();
    if (np == NULL)
    {
        return -ENOMEM;
    }

    struct process *parent = get_current();

    // Copy memory
    if (proc_copy_memory(parent, np) == -1)
    {
        proc_put(np);
        return -ENOMEM;
    }

    // Copy registers
    *(np->trapframe) = *(parent->trapframe);
    // Cause fork to return 0 in the child.
    trapframe_set_return_register(np->trapframe, 0);

    // Copy open files:
    // Increment reference counts on open file descriptors including the curent
    // working directory
    for (size_t i = 0; i < MAX_FILES_PER_PROCESS; i++)
    {
        if (parent->files[i])
        {
            np->files[i] = file_dup(parent->files[i]);
        }
    }
    np->cwd = VFS_INODE_DUP(parent->cwd);

    // Copy name
    safestrcpy(np->name, parent->name, sizeof(parent->name));

    pid_t pid = np->pid;

    spin_unlock(&np->lock);

    spin_lock(&g_wait_lock);
    np->parent = parent;
    spin_unlock(&g_wait_lock);

    spin_lock(&np->lock);
    np->state = RUNNABLE;
    np->debug_log_depth = 0;
    spin_unlock(&np->lock);

    // add to kobject tree
    kobject_add(&np->kobj, &g_kobjects_proc, "%d", np->pid);
    proc_put(np);  // drop reference now that the kobject tree holds one

    // add to process list
    rwspin_write_lock(&g_process_list.lock);
    list_add_tail(&np->plist, &g_process_list.plist);
    rwspin_write_unlock(&g_process_list.lock);

    return pid;
}

/// Pass proc's abandoned children to init.
/// Caller must hold g_wait_lock.
void reparent(struct process *proc)
{
    struct list_head *pos;
    rwspin_read_lock(&g_process_list.lock);
    list_for_each(pos, &g_process_list.plist)
    {
        struct process *pp = process_from_list(pos);
        if (pp->parent == proc)
        {
            pp->parent = g_initial_user_process;
            wakeup_holding_plist_lock(g_initial_user_process);
        }
    }
    rwspin_read_unlock(&g_process_list.lock);
}

/// Exit the current process.  Does not return.
/// An exited process remains in the zombie state
/// until its parent calls wait().
void exit(int32_t status)
{
    struct process *proc = get_current();

    // special case: "/usr/bin/init" returned
    if (proc == g_initial_user_process)
    {
        size_t return_value = trapframe_get_return_register(proc->trapframe);

        printk("/usr/bin/init returned: %zd\n", return_value);
        panic("/usr/bin/init should not have returned");
    }

    // Close all open files.
    for (FILE_DESCRIPTOR fd = 0; fd < MAX_FILES_PER_PROCESS; fd++)
    {
        if (proc->files[fd])
        {
            struct file *f = proc->files[fd];
            file_close(f);
            proc->files[fd] = NULL;
        }
    }

    inode_put(proc->cwd);
    proc->cwd = NULL;

    spin_lock(&g_wait_lock);

    // Give any children to init.
    reparent(proc);

    rwspin_read_lock(&g_process_list.lock);
    // Parent might be sleeping in wait().
    // Note that the parent can't free the process while we still hold
    // the proc->lock, because it will acquire the lock before the free.
    spin_lock(&proc->lock);
    wakeup_holding_plist_lock(proc->parent);
    rwspin_read_unlock(&g_process_list.lock);

    proc->xstate = status;
    proc->state = ZOMBIE;

    spin_unlock(&g_wait_lock);

    // Jump into the scheduler, never to return.
    sched();
    panic("zombie exit");
}

pid_t wait(int32_t *wstatus)
{
    struct process *proc = get_current();

    spin_lock(&g_wait_lock);

    for (;;)
    {
        // Scan through table looking for exited children.
        bool havekids = false;
        struct list_head *pos;
        rwspin_write_lock(&g_process_list.lock);
        list_for_each(pos, &g_process_list.plist)
        {
            struct process *pp = process_from_list(pos);

            // we can only wait on our own children:
            if (pp->parent == proc)
            {
                // make sure the child isn't still in exit() or
                // context_switch().
                spin_lock(&pp->lock);

                havekids = true;
                if (pp->state == ZOMBIE)
                {
                    // Found one.
                    pid_t pid = pp->pid;
                    if (wstatus != NULL &&
                        uvm_copy_out(proc->pagetable, (size_t)wstatus,
                                     (char *)&pp->xstate,
                                     sizeof(pp->xstate)) < 0)
                    {
                        // error copying out status
                        spin_unlock(&pp->lock);
                        spin_unlock(&g_wait_lock);

                        rwspin_write_unlock(&g_process_list.lock);
                        return -EFAULT;
                    }

                    // remove from process list, lock is already held
                    list_del(&pp->plist);

                    // remove from kobject tree, if that was the last reference,
                    // proc_free() will be called
                    kobject_del(&pp->kobj);

                    spin_unlock(&g_wait_lock);

                    rwspin_write_unlock(&g_process_list.lock);
                    return pid;
                }
                spin_unlock(&pp->lock);
            }
        }
        rwspin_write_unlock(&g_process_list.lock);

        // No point waiting if we don't have any children.
        if (!havekids || proc_is_killed(proc))
        {
            spin_unlock(&g_wait_lock);
            return -ECHILD;
        }

        // Wait for a child to exit.
        sleep(proc, &g_wait_lock);
    }
}

/// Switch to scheduler.  Must hold only proc->lock
/// and have changed proc->state. Saves and restores
/// disable_dev_int_stack_original_state because
/// disable_dev_int_stack_original_state is a property of this kernel thread,
/// not this CPU. It should be proc->disable_dev_int_stack_original_state and
/// proc->disable_dev_int_stack_depth, but that would break in the few places
/// where a lock is held but there's no process.
void sched()
{
    struct process *proc = get_current();
    DEBUG_ASSERT_CPU_HOLDS_LOCK(&proc->lock);

    if (get_cpu()->disable_dev_int_stack_depth != 1)
    {
        printk(
            "ERROR: CPU %zd disable_dev_int_stack_depth "
            "is %d instead of 1\n",
            smp_processor_id(), get_cpu()->disable_dev_int_stack_depth);
        panic("sched invalid disable_dev_int_stack_depth");
    }

    if (proc->state == RUNNING)
    {
        panic("sched process is already running");
    }

    if (cpu_is_interrupts_enabled())
    {
        panic("sched interruptible");
    }

    bool state_before_switch = get_cpu()->disable_dev_int_stack_original_state;
    context_switch(&proc->context, &get_cpu()->context);
    get_cpu()->disable_dev_int_stack_original_state = state_before_switch;
}

/// Give up the CPU for one scheduling round.
void yield()
{
    struct process *proc = get_current();
    spin_lock(&proc->lock);
    proc->state = RUNNABLE;
    sched();
    spin_unlock(&proc->lock);
}

/// @brief Called once to load the first process from forkret() into the
/// currently active process. panics on error.
/// @param init_path Absolute path to the init binary.
void load_init_process(char *init_path)
{
    get_current()->cwd = inode_from_path("/");
    int ret = execv(init_path, (char *[]){init_path, 0});
    if (ret < 0)
    {
        switch (ret)
        {
            case -ENOENT:
                printk("ERROR starting init process, binary not found at %s\n",
                       init_path);
                break;
            case -ENOEXEC:
                printk("ERROR starting init process, %s is not an executable\n",
                       init_path);
                break;
            case -ENOMEM:
                printk(
                    "ERROR starting init process: out of memory while loading "
                    "%s\n",
                    init_path);
                break;

            default: break;
        }
        panic("execv of init failed");
    }
}

/// A fork child's very first scheduling by scheduler()
/// will context_switch to forkret.
void forkret()
{
    static bool first = true;

    // Still holding p->lock from scheduler.
    spin_unlock(&get_current()->lock);

    if (first)
    {
        // File system initialization must be run in the context of a
        // regular process (e.g., because it calls sleep), and thus cannot
        // be run from main().
        first = false;
        mount_root(ROOT_DEVICE_NUMBER, XV6_FS_NAME);
        printk("forkret() mounting /... OK\n");

        // We can involve execv() after file system is initialized.
        char *init_path = "/usr/bin/init";
        load_init_process(init_path);
        printk("forkret() loading %s... OK\n", init_path);

        atomic_thread_fence(
            memory_order_seq_cst);  // other cores must see first = false
    }

    return_to_user_mode();
}

void sleep(void *chan, struct spinlock *lk)
{
    struct process *proc = get_current();

    // Must acquire p->lock in order to
    // change p->state and then call sched.
    // Once we hold p->lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup locks p->lock),
    // so it's okay to release lk.

    spin_lock(&proc->lock);
    spin_unlock(lk);

    // Go to sleep.
    proc->chan = chan;
    proc->state = SLEEPING;

    sched();

    // Tidy up.
    proc->chan = NULL;

    // Reacquire original lock.
    spin_unlock(&proc->lock);
    spin_lock(lk);
}

void wakeup_holding_plist_lock(void *chan)
{
    struct process *current_process = get_current();

    // if (chan != &g_ticks) printk("wakeup 0x%zx\n", (size_t)chan);

    struct list_head *pos;
    list_for_each(pos, &g_process_list.plist)
    {
        struct process *proc = process_from_list(pos);

        if (proc != current_process)
        {
            spin_lock(&proc->lock);
            if (proc->state == SLEEPING && proc->chan == chan)
            {
                proc->state = RUNNABLE;
            }
            spin_unlock(&proc->lock);
        }
    }
}

void wakeup(void *chan)
{
    spin_lock(&g_wait_lock);
    rwspin_read_lock(&g_process_list.lock);
    wakeup_holding_plist_lock(chan);
    rwspin_read_unlock(&g_process_list.lock);
    spin_unlock(&g_wait_lock);
}

/// Kill the process with the given pid.
/// The victim won't exit until it tries to return
/// to user space (see user_mode_interrupt_handler() in trap.c).
ssize_t proc_send_signal(pid_t pid, int32_t sig)
{
    if (sig != SIGKILL)
    {
        // no other signals are supported so far
        return -EINVAL;
    }

    struct list_head *pos;
    rwspin_read_lock(&g_process_list.lock);
    list_for_each(pos, &g_process_list.plist)
    {
        struct process *proc = process_from_list(pos);

        spin_lock(&proc->lock);
        if (proc->pid == pid)
        {
            proc->killed = true;
            if (proc->state == SLEEPING)
            {
                // Wake process from sleep().
                proc->state = RUNNABLE;
            }
            spin_unlock(&proc->lock);

            rwspin_read_unlock(&g_process_list.lock);
            return 0;
        }
        spin_unlock(&proc->lock);
    }

    rwspin_read_unlock(&g_process_list.lock);
    return -ESRCH;
}

void proc_set_killed(struct process *proc)
{
    spin_lock(&proc->lock);
    proc->killed = true;
    spin_unlock(&proc->lock);
}

bool proc_is_killed(struct process *proc)
{
    spin_lock(&proc->lock);
    bool is_killed = proc->killed;
    spin_unlock(&proc->lock);
    return is_killed;
}

bool proc_grow_stack(struct process *proc)
{
    size_t stack_size = USER_STACK_HIGH - proc->stack_low;
    if (stack_size >= USER_MAX_STACK_SIZE)
    {
        printk("proc_grow_stack: don't want to grow stack anymore\n");
        return false;
    }
    size_t low = uvm_grow_stack(proc->pagetable, proc->stack_low);
    if (low == 0)
    {
        printk("proc_grow_stack: can't grow stack anymore\n");
        return false;
    }
    proc->stack_low = low;
    return true;
}

void proc_shrink_stack(struct process *proc)
{
    // always keep 1 page:
    if (proc->stack_low >= (USER_STACK_HIGH - PAGE_SIZE)) return;

    size_t lowest_stack_page_used = PAGE_ROUND_DOWN(proc->trapframe->sp);
    if (lowest_stack_page_used <= proc->stack_low) return;  // all pages in use

    size_t npages = (lowest_stack_page_used - proc->stack_low) / PAGE_SIZE;

    uvm_unmap(proc->pagetable, proc->stack_low, npages, true);
    proc->stack_low = lowest_stack_page_used;
}

int either_copyout(bool addr_is_userspace, size_t dst, void *src, size_t len)
{
    struct process *proc = get_current();
    if (addr_is_userspace)
    {
        return uvm_copy_out(proc->pagetable, dst, src, len);
    }
    else
    {
        memmove((char *)dst, src, len);
        return 0;
    }
}

int either_copyin(void *dst, bool addr_is_userspace, size_t src, size_t len)
{
    struct process *proc = get_current();
    if (addr_is_userspace)
    {
        return uvm_copy_in(proc->pagetable, (char *)dst, src, len);
    }
    else
    {
        memmove(dst, (char *)src, len);
        return 0;
    }
}

void debug_print_call_stack_kernel(struct process *proc)
{
    size_t proc_stack = proc->kstack;

    size_t frame_pointer = context_get_frame_pointer(&proc->context);
    size_t return_address = context_get_return_register(&proc->context);

    do {
        printk("  ra (kernel): " FORMAT_REG_SIZE "\n", return_address);

        return_address = *((size_t *)(frame_pointer - 1 * sizeof(size_t)));
        // stack_pointer = frame_pointer;
        frame_pointer = *((size_t *)(frame_pointer - 2 * sizeof(size_t)));
    } while ((frame_pointer <= proc_stack + PAGE_SIZE) &&
             (frame_pointer > proc_stack));
}

void debug_print_call_stack_kernel_fp(size_t frame_pointer)
{
    size_t depth = 32;  // limit just in case of a corrupted stack
    while (depth-- != 0)
    {
        if (frame_pointer == 0) break;
        size_t ra_address = frame_pointer - 1 * sizeof(size_t);
        if (kvm_get_physical_paddr(ra_address) == 0)
        {
            printk("  ra: <invalid address>\n");
            break;
        }
        size_t ra = *((size_t *)(ra_address));
        size_t next_frame_pointer_address = frame_pointer - 2 * sizeof(size_t);
        if (kvm_get_physical_paddr(next_frame_pointer_address) == 0)
        {
            printk("  invalid frame pointer address: 0x%zx\n",
                   next_frame_pointer_address);
            break;
        }
        frame_pointer = *((size_t *)(next_frame_pointer_address));
        printk("  ra: " FORMAT_REG_SIZE "\n", ra);
    };
}

static inline bool address_is_in_page(size_t addr, size_t page_address)
{
    return (addr >= page_address && addr < (page_address + PAGE_SIZE));
}

void debug_print_call_stack_user(struct process *proc)
{
    // TODO: only goes over the first stack page!
    size_t proc_stack_pa =
        uvm_get_physical_addr(proc->pagetable, proc->stack_low, NULL);

    size_t frame_pointer = trapframe_get_frame_pointer(proc->trapframe);
    size_t fp_physical =
        uvm_get_physical_addr(proc->pagetable, frame_pointer, NULL);
    size_t return_address = trapframe_get_return_address(proc->trapframe);

    if (proc_stack_pa == 0 || fp_physical == 0)
    {
        printk("<no user stack mapped>\n");
        return;
    }

    while (address_is_in_page(fp_physical, proc_stack_pa))
    {
        printk("  ra (user): " FORMAT_REG_SIZE "\n", return_address);

        return_address = *((size_t *)(fp_physical - 1 * sizeof(size_t)));
        frame_pointer = *((size_t *)(fp_physical - 2 * sizeof(size_t)));
        fp_physical =
            uvm_get_physical_addr(proc->pagetable, frame_pointer, NULL);
    };
}

void debug_print_open_files(struct process *proc)
{
    for (size_t i = 0; i < MAX_FILES_PER_PROCESS; ++i)
    {
        struct file *f = proc->files[i];
        if (f != NULL && f->ip != NULL)
        {
            struct inode *ip = proc->files[i]->ip;
            printk("  fd %zd (ref# %d, off: %d): ", i, kref_read(&f->ref),
                   f->off);
            debug_print_inode(ip);
            printk("\n");
        }
    }
}

void debug_print_process(bool print_call_stack_user,
                         bool print_call_stack_kernel, bool print_files,
                         bool print_page_table, struct process *proc)
{
    static char *states[] = {[USED] "used",
                             [SLEEPING] "sleeping",
                             [RUNNABLE] "runnable",
                             [RUNNING] "running",
                             [ZOMBIE] "zombie"};

    char *state = "???";
    if (proc->state >= 0 && proc->state < NELEM(states) && states[proc->state])
    {
        state = states[proc->state];
    }

    printk(" PID: %d", proc->pid);

    if (proc->parent)
    {
        printk(" (PPID: %d)", proc->parent->pid);
    }
    printk(" | %s", proc->name);
    printk(" | cwd: ");
    debug_print_inode(proc->cwd);
    printk(" | state: %s", state);

    if (proc->state == ZOMBIE)
    {
        printk(" (return value: %d)", proc->xstate);
    }
    if (proc->state == SLEEPING)
    {
        printk(", waiting on: ");
        bool found_chan = false;
        if (proc->chan == proc)
        {
            printk("child");
            found_chan = true;
        }
        else if (proc->chan == &g_ticks)
        {
            printk("timer");
            found_chan = true;
        }
        if (!found_chan)
        {
            printk("0x%zx", (size_t)proc->chan);
        }
    }
#ifdef CONFIG_DEBUG
    if (proc->current_syscall != 0)
    {
        printk(" | in syscall %s",
               debug_get_syscall_name(proc->current_syscall));
    }
#endif
    printk("\n");

    if (print_call_stack_user && proc->state != RUNNING)
    {
        printk("Call stack user:\n");
        debug_print_call_stack_user(proc);
    }
    if (print_call_stack_kernel && proc->state != RUNNING)
    {
        printk("Call stack kernel:\n");
        debug_print_call_stack_kernel(proc);
    }
    if (print_files)
    {
        printk("Open files:\n");
        debug_print_open_files(proc);
    }
    if (print_page_table)
    {
        debug_vm_print_page_table(proc->pagetable);
    }
}

void debug_print_process_list(bool print_call_stack_user,
                              bool print_call_stack_kernel, bool print_files,
                              bool print_page_table)
{
    printk("\nProcess list (%zd)\n", (size_t)smp_processor_id());

    struct list_head *pos;
    rwspin_read_lock(&g_process_list.lock);
    list_for_each(pos, &g_process_list.plist)
    {
        struct process *proc = process_from_list(pos);
        debug_print_process(print_call_stack_user, print_call_stack_kernel,
                            print_files, print_page_table, proc);
    }
    rwspin_read_unlock(&g_process_list.lock);
}

FILE_DESCRIPTOR fd_alloc(struct file *f)
{
    struct process *proc = get_current();

    for (size_t fd = 0; fd < MAX_FILES_PER_PROCESS; fd++)
    {
        if (proc->files[fd] == NULL)
        {
            proc->files[fd] = f;
            return (FILE_DESCRIPTOR)fd;
        }
    }
    return INVALID_FILE_DESCRIPTOR;
}
