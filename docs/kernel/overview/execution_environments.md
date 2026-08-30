# Execution Environments

## Boot Environment

- **Stack:** None, then per-CPU stack the core booted on.
- **Page table:** None, then early kernel page table, then kernel page table
- **Code run:** Kernel, might start in machine mode before entering kernel mode

During early boot all cores create and then run on a stack located in the kernels BBS section. This stack is used during booting and when entering the scheduler.

Initially a simplified "early page table" gets created and enabled. When entering `main()` the threads can assume that all kernel code and data is mapped. Soon after the remaining memory regions are detected and mapped.

The boot always ends in entering the [scheduler](../processes/scheduling.md).

## M-Mode Environment

**Only on RISC V with embedded [SBI](../../riscv/SBI.md) compile option.**

- **Stack:** Per-CPU M-Mode stack.
    - Statically allocated, fixed size
- **Page table:** Previous page table
- **Code run:** SBI, machine mode

Interrupts handled by the firmware should be invisible to the interrupted environment.

## Scheduler Environment

- **Stack:** Per-CPU stack the core booted on.
    - Statically allocated, fixed size
- **Page table:** Kernel page table
- **Code run:** Kernel, kernel mode

Used during boot, interrupts and while scheduling.

**Leave:**

- Switch to a **[Process](../processes/processes.md) Kernel Environment** via `context_switch`.
    - Store current **Scheduler Environment** in per-CPU `struct context`.
    - Load **Process Kernel Environment** from per-process `struct context`.
- Interrupt via `s_mode_trap_vector`.
    - Save the environment on current kernel stack
    - Execute `kernel_mode_interrupt_handler`

## Process Kernel Environment

- **Stack:** Per-Process kernel stack
    - Dynamically allocated, fixed size
- **Page table:** Kernel page table
- **Code run:** Kernel, kernel mode

Used during [syscalls](../syscalls/syscalls.md), interrupts and implicitly after creating a [process](../processes/processes.md) as it will start in `forkret()`.

**Leave:**

- Via `return_to_user_mode()`
    - Load **Process User Environment** from `Trapframe`.
    - Store minimal **Process Kernel Environment** (_Stack+PT_)
- Interrupts via `s_mode_trap_vector`
    - Save the environment on current kernel stack
    - Execute `kernel_mode_interrupt_handler`.
- Via `context_switch()` (after `yield()`).
    - Store **Process Kernel Environment** in per-process `struct context`.
    - Load **Scheduler Environment** from per-CPU `struct context`.

## Process User Environment

- **Stack:** Per-Process user stack
    - Dynamically allocated, can grow
- **Page table:** User page table
- **Code run:** Process, user mode

**Leave:**

- Intentionally via [syscalls](../syscalls/syscalls.md) / `u_mode_trap_vector`
    - Load minimal **Process Kernel Environment** (_Stack+PT_)
    - Store **Process User Environment** in `Trapframe`.
- Interrupts via `u_mode_trap_vector`
    - Load minimal **Process Kernel Environment** (_Stack+PT_)
    - Store **Process User Environment** in `Trapframe`.

_Stack+PT_:

- Kernel stack
- Kernel page table
- No saved registers to save/restore
