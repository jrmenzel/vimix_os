/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/trap.h>
#include <fs/xv6fs/log.h>
#include <kernel/cpu.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/signal.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/vm.h>
#include <mm/memlayout.h>

/// a user program that calls execv("/init")
/// assembled from initcode.S
/// od -t xC initcode
#include <arch/riscv/asm/initcode.h>
#define g_initcode ___build_kernel_arch_riscv_asm_initcode
_Static_assert((sizeof(g_initcode) <= PAGE_SIZE),
               "Initcode must fit into one page");

struct cpu g_cpus[MAX_CPUS];

/// @brief All user processes (except for init)
struct process g_process_list[MAX_PROCS];

/// @brief The init process in user mode.
/// Created in userspace_init(), the only process not created by fork()
struct process *g_initial_user_process;

pid_t g_next_pid = 1;
struct spinlock g_pid_lock;

extern void forkret();
static void free_process(struct process *proc);

extern char trampoline[];  // u_mode_trap_vector.S

/// helps ensure that wakeups of wait()ing
/// parents are not lost. helps obey the
/// memory model when using p->parent.
/// must be acquired before any p->lock.
struct spinlock g_wait_lock;

/// Allocate a page for each process's kernel stack.
/// Map it high in memory, followed by an invalid
/// guard page.
void init_per_process_kernel_stack(pagetable_t kpage_table)
{
    struct process *proc;

    for (proc = g_process_list; proc < &g_process_list[MAX_PROCS]; proc++)
    {
        char *pa = kalloc();
        if (pa == 0) panic("kalloc");
        size_t va = KSTACK((int)(proc - g_process_list));
        kvm_map_or_panic(kpage_table, va, (size_t)pa, PAGE_SIZE, PTE_R | PTE_W);
    }
}

/// initialize the proc table.
void proc_init()
{
    struct process *proc;

    spin_lock_init(&g_pid_lock, "nextpid");
    spin_lock_init(&g_wait_lock, "wait_lock");
    for (proc = g_process_list; proc < &g_process_list[MAX_PROCS]; proc++)
    {
        spin_lock_init(&proc->lock, "proc");
        proc->state = UNUSED;
        proc->kstack = KSTACK((int)(proc - g_process_list));
    }
}

/// Must be called with interrupts disabled,
/// to prevent race with process being moved
/// to a different CPU.
int smp_processor_id()
{
    int id = __arch_smp_processor_id();
    return id;
}

/// Return this CPU's cpu struct.
/// Interrupts must be disabled as long as the
/// returned struct is used (as a context switch
/// invalidates the cpu if the kernel process switched cores)
struct cpu *get_cpu()
{
#if defined(CONFIG_DEBUG_EXTRA_RUNTIME_TESTS)
    if (cpu_is_device_interrupts_enabled())
    {
        panic("interrupts must be disabled when calling get_cpu");
    }
#endif  // CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
    size_t id = smp_processor_id();
    struct cpu *c = &g_cpus[id];
    return c;
}

/// Return the current struct process *, or NULL if none.
struct process *get_current()
{
    cpu_push_disable_device_interrupt_stack();
    struct cpu *c = get_cpu();
    struct process *proc = c->proc;
    cpu_pop_disable_device_interrupt_stack();
    return proc;
}

/// Get a new unique process ID
pid_t alloc_pid()
{
    spin_lock(&g_pid_lock);
    pid_t pid = g_next_pid;
    g_next_pid = g_next_pid + 1;
    spin_unlock(&g_pid_lock);

    return pid;
}

/// Look in the process table for an UNUSED proc.
/// If found, initialize state required to run in the kernel,
/// and return with p->lock held.
/// If there are no free procs, or a memory allocation fails, return 0.
static struct process *alloc_process()
{
    struct process *proc;

    for (proc = g_process_list; proc < &g_process_list[MAX_PROCS]; proc++)
    {
        spin_lock(&proc->lock);
        if (proc->state == UNUSED)
        {
            goto found;
        }
        else
        {
            spin_unlock(&proc->lock);
        }
    }
    return 0;

found:
    proc->pid = alloc_pid();
    proc->state = USED;

    // Allocate a trapframe page.
    if ((proc->trapframe = (struct trapframe *)kalloc()) == 0)
    {
        free_process(proc);
        spin_unlock(&proc->lock);
        return 0;
    }

    // An empty user page table.
    proc->pagetable = proc_pagetable(proc);
    if (proc->pagetable == 0)
    {
        free_process(proc);
        spin_unlock(&proc->lock);
        return 0;
    }

    // Set up new context to start executing at forkret,
    // which returns to user space.
    memset(&proc->context, 0, sizeof(proc->context));
    proc->context.ra = (size_t)forkret;
    proc->context.sp = proc->kstack + PAGE_SIZE;

    return proc;
}

/// free a proc structure and the data hanging from it,
/// including user pages.
/// p->lock must be held.
static void free_process(struct process *proc)
{
    if (proc->trapframe) kfree((void *)proc->trapframe);
    proc->trapframe = 0;
    if (proc->pagetable) proc_free_pagetable(proc->pagetable, proc->sz);
    proc->pagetable = 0;
    proc->sz = 0;
    proc->pid = 0;
    proc->parent = 0;
    proc->name[0] = 0;
    proc->chan = 0;
    proc->killed = 0;
    proc->xstate = 0;
    proc->state = UNUSED;
}

/// Create a user page table for a given process, with no user memory,
/// but with trampoline and trapframe pages.
pagetable_t proc_pagetable(struct process *proc)
{
    pagetable_t pagetable;

    // An empty page table.
    pagetable = uvm_create();
    if (pagetable == 0) return 0;

    // map the trampoline code (for system call return)
    // at the highest user virtual address.
    // only the supervisor uses it, on the way
    // to/from user space, so not PTE_U.
    if (kvm_map(pagetable, TRAMPOLINE, PAGE_SIZE, (size_t)trampoline,
                PTE_R | PTE_X) < 0)
    {
        uvm_free(pagetable, 0);
        return 0;
    }

    // map the trapframe page just below the trampoline page, for
    // u_mode_trap_vector.S.
    if (kvm_map(pagetable, TRAPFRAME, PAGE_SIZE, (size_t)(proc->trapframe),
                PTE_R | PTE_W) < 0)
    {
        uvm_unmap(pagetable, TRAMPOLINE, 1, 0);
        uvm_free(pagetable, 0);
        return 0;
    }

    return pagetable;
}

/// Free a process's page table, and free the
/// physical memory it refers to.
void proc_free_pagetable(pagetable_t pagetable, size_t sz)
{
    uvm_unmap(pagetable, TRAMPOLINE, 1, 0);
    uvm_unmap(pagetable, TRAPFRAME, 1, 0);
    uvm_free(pagetable, sz);
}

/// Set up first user process.
/// This creates the only process not created by fork()
void userspace_init()
{
    struct process *proc = alloc_process();
    g_initial_user_process = proc;

    // Allocate one user page and load the user initcode into
    // address 0 of pagetable
    char *mem = kalloc();
    memset(mem, 0, PAGE_SIZE);
    kvm_map(proc->pagetable, 0, PAGE_SIZE, (size_t)mem,
            PTE_W | PTE_R | PTE_X | PTE_U);
    memmove(mem, g_initcode, sizeof(g_initcode));

    proc->sz = PAGE_SIZE;

    // prepare for the very first "return" from kernel to user.
    proc->trapframe->epc = 0;         // user program counter
    proc->trapframe->sp = PAGE_SIZE;  // user stack pointer

    safestrcpy(proc->name, "initcode", sizeof(proc->name));
    proc->cwd = inode_from_path("/");

    proc->state = RUNNABLE;

    spin_unlock(&proc->lock);
}

/// Grow or shrink user memory by n bytes.
/// Return 0 on success, -1 on failure.
int32_t proc_grow_memory(ssize_t n)
{
    struct process *proc = get_current();
    size_t sz = proc->sz;

    if (n > 0)
    {
        if ((sz = uvm_alloc(proc->pagetable, sz, sz + n, PTE_W)) == 0)
        {
            return -1;
        }
    }
    else if (n < 0)
    {
        sz = uvm_dealloc(proc->pagetable, sz, sz + n);
    }
    proc->sz = sz;
    return 0;
}

int32_t fork()
{
    int i, pid;
    struct process *np;
    struct process *proc = get_current();

    // Allocate process.
    if ((np = alloc_process()) == NULL)
    {
        return -1;
    }

    // Copy user memory from parent to child.
    if (uvm_copy(proc->pagetable, np->pagetable, proc->sz) < 0)
    {
        free_process(np);
        spin_unlock(&np->lock);
        return -1;
    }
    np->sz = proc->sz;

    // copy saved user registers.
    *(np->trapframe) = *(proc->trapframe);

    // Cause fork to return 0 in the child.
    np->trapframe->a0 = 0;

    // increment reference counts on open file descriptors.
    for (i = 0; i < MAX_FILES_PER_PROCESS; i++)
        if (proc->files[i]) np->files[i] = file_dup(proc->files[i]);
    np->cwd = inode_dup(proc->cwd);

    safestrcpy(np->name, proc->name, sizeof(proc->name));

    pid = np->pid;

    spin_unlock(&np->lock);

    spin_lock(&g_wait_lock);
    np->parent = proc;
    spin_unlock(&g_wait_lock);

    spin_lock(&np->lock);
    np->state = RUNNABLE;
    spin_unlock(&np->lock);

    return pid;
}

/// Pass p's abandoned children to init.
/// Caller must hold g_wait_lock.
void reparent(struct process *proc)
{
    struct process *pp;

    for (pp = g_process_list; pp < &g_process_list[MAX_PROCS]; pp++)
    {
        if (pp->parent == proc)
        {
            pp->parent = g_initial_user_process;
            wakeup(g_initial_user_process);
        }
    }
}

/// Exit the current process.  Does not return.
/// An exited process remains in the zombie state
/// until its parent calls wait().
void exit(int32_t status)
{
    struct process *proc = get_current();

    // special case: "/init" or even initcode.S returned
    if (proc == g_initial_user_process)
    {
        size_t return_value = proc->trapframe->a0;
        if (return_value == -0xDEAD)
        {
            panic("initcode.S could not load /init - check filesystem");
        }

        printk("/init returned: %d\n", return_value);
        panic("/init should not have returned");
    }

    // Close all open files.
    for (int fd = 0; fd < MAX_FILES_PER_PROCESS; fd++)
    {
        if (proc->files[fd])
        {
            struct file *f = proc->files[fd];
            file_close(f);
            proc->files[fd] = 0;
        }
    }

    log_begin_fs_transaction();
    inode_put(proc->cwd);
    log_end_fs_transaction();
    proc->cwd = 0;

    spin_lock(&g_wait_lock);

    // Give any children to init.
    reparent(proc);

    // Parent might be sleeping in wait().
    wakeup(proc->parent);

    spin_lock(&proc->lock);

    proc->xstate = status;
    proc->state = ZOMBIE;

    spin_unlock(&g_wait_lock);

    // Jump into the scheduler, never to return.
    sched();
    panic("zombie exit");
}

/// Wait for a child process to exit and return its pid.
/// Return -1 if this process has no children.
int wait(int32_t *wstatus)
{
    struct process *pp;
    int havekids, pid;
    struct process *proc = get_current();

    spin_lock(&g_wait_lock);

    for (;;)
    {
        // Scan through table looking for exited children.
        havekids = 0;
        for (pp = g_process_list; pp < &g_process_list[MAX_PROCS]; pp++)
        {
            if (pp->parent == proc)
            {
                // make sure the child isn't still in exit() or
                // context_switch().
                spin_lock(&pp->lock);

                havekids = 1;
                if (pp->state == ZOMBIE)
                {
                    // Found one.
                    pid = pp->pid;
                    if (wstatus != NULL &&
                        uvm_copy_out(proc->pagetable, (size_t)wstatus,
                                     (char *)&pp->xstate,
                                     sizeof(pp->xstate)) < 0)
                    {
                        spin_unlock(&pp->lock);
                        spin_unlock(&g_wait_lock);
                        return -1;
                    }
                    free_process(pp);
                    spin_unlock(&pp->lock);
                    spin_unlock(&g_wait_lock);
                    return pid;
                }
                spin_unlock(&pp->lock);
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || proc_is_killed(proc))
        {
            spin_unlock(&g_wait_lock);
            return -1;
        }

        // Wait for a child to exit.
        sleep(proc, &g_wait_lock);
    }
}

/// Switch to scheduler.  Must hold only p->lock
/// and have changed proc->state. Saves and restores
/// disable_dev_int_stack_original_state because
/// disable_dev_int_stack_original_state is a property of this kernel thread,
/// not this CPU. It should be proc->disable_dev_int_stack_original_state and
/// proc->disable_dev_int_stack_depth, but that would break in the few places
/// where a lock is held but there's no process.
void sched()
{
    struct process *proc = get_current();

    if (!spin_lock_is_held_by_this_cpu(&proc->lock)) panic("sched p->lock");
    if (get_cpu()->disable_dev_int_stack_depth != 1) panic("sched locks");
    if (proc->state == RUNNING) panic("sched running");
    if (cpu_is_device_interrupts_enabled()) panic("sched interruptible");

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

/// A fork child's very first scheduling by scheduler()
/// will context_switch to forkret.
void forkret()
{
    static int first = 1;

    // Still holding p->lock from scheduler.
    spin_unlock(&get_current()->lock);

    if (first)
    {
        // File system initialization must be run in the context of a
        // regular process (e.g., because it calls sleep), and thus cannot
        // be run from main().
        first = 0;
        init_root_file_system(ROOT_DEVICE_NUMBER);
    }

    return_to_user_mode();
}

/// Atomically release lock and sleep on chan.
/// Reacquires lock when awakened.
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
    proc->chan = 0;

    // Reacquire original lock.
    spin_unlock(&proc->lock);
    spin_lock(lk);
}

/// Wake up all processes sleeping on chan.
/// Must be called without any p->lock.
void wakeup(void *chan)
{
    struct process *proc;

    for (proc = g_process_list; proc < &g_process_list[MAX_PROCS]; proc++)
    {
        if (proc != get_current())
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

/// Kill the process with the given pid.
/// The victim won't exit until it tries to return
/// to user space (see user_mode_interrupt_handler() in trap.c).
int32_t proc_send_signal(pid_t pid, int32_t sig)
{
    if (sig != SIGKILL)
    {
        // no other signals are supported so far
        return -1;
    }

    for (struct process *proc = g_process_list;
         proc < &g_process_list[MAX_PROCS]; proc++)
    {
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
            return 0;
        }
        spin_unlock(&proc->lock);
    }
    return -1;
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

/// Print a process listing to console.  For debugging.
/// Runs when user types ^P on console.
/// No lock to avoid wedging a stuck machine further.
void debug_print_process_list()
{
    static char *states[] = {
        [UNUSED] "unused",   [USED] "used",      [SLEEPING] "sleep ",
        [RUNNABLE] "runble", [RUNNING] "run   ", [ZOMBIE] "zombie"};
    struct process *proc;
    char *state;

    printk("\n");
    for (proc = g_process_list; proc < &g_process_list[MAX_PROCS]; proc++)
    {
        if (proc->state == UNUSED) continue;
        if (proc->state >= 0 && proc->state < NELEM(states) &&
            states[proc->state])
            state = states[proc->state];
        else
            state = "???";
        printk("%d %s %s", proc->pid, state, proc->name);
        printk("\n");
    }
}

int fd_alloc(struct file *f)
{
    int fd;
    struct process *proc = get_current();

    for (fd = 0; fd < MAX_FILES_PER_PROCESS; fd++)
    {
        if (proc->files[fd] == 0)
        {
            proc->files[fd] = f;
            return fd;
        }
    }
    return -1;
}
