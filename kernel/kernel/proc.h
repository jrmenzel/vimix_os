#include <arch/context.h>

/// Per-CPU state.
struct cpu
{
    struct proc *proc;       // The process running on this cpu, or null.
    struct context context;  // swtch() here to enter scheduler().
    int noff;                // Depth of push_off() nesting.
    int intena;              // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

enum procstate
{
    UNUSED,
    USED,
    SLEEPING,
    RUNNABLE,
    RUNNING,
    ZOMBIE
};

/// Per-process state
struct proc
{
    struct spinlock lock;

    // p->lock must be held when using these:
    enum procstate state;  ///< Process state
    void *chan;            ///< If non-zero, sleeping on chan
    int killed;            ///< If non-zero, have been killed
    int xstate;            ///< Exit status to be returned to parent's wait
    int pid;               ///< Process ID

    // wait_lock must be held when using this:
    struct proc *parent;  ///< Parent process

    // these are private to the process, so p->lock need not be held.
    uint64 kstack;                ///< Virtual address of kernel stack
    uint64 sz;                    ///< Size of process memory (bytes)
    pagetable_t pagetable;        ///< User page table
    struct trapframe *trapframe;  ///< data page for trampoline.S
    struct context context;       ///< swtch() here to run process
    struct file *ofile[NOFILE];   ///< Open files
    struct inode *cwd;            ///< Current directory
    char name[16];                ///< Process name (debugging)
};
