# SPDX-License-Identifier: MIT

#if (__riscv_xlen == 32)
#define SIZE_OF_REG 4
#define SAVE_REG_TO_SCRATCH(REGISTER, OFFSET) sw REGISTER, (OFFSET*SIZE_OF_REG)(a0)
#define LOAD_REG_FROM_SCRATCH(REGISTER, OFFSET) lw REGISTER, (OFFSET*SIZE_OF_REG)(a0)
#define MACHINE_TIMER_INTERRUPT (1<<31 | 7)
#else 
# 64 bit
#define SIZE_OF_REG 8
#define SAVE_REG_TO_SCRATCH(REGISTER, OFFSET) sd REGISTER, (OFFSET*SIZE_OF_REG)(a0)
#define LOAD_REG_FROM_SCRATCH(REGISTER, OFFSET) ld REGISTER, (OFFSET*SIZE_OF_REG)(a0)
#define MACHINE_TIMER_INTERRUPT (1<<63 | 7)
#endif

# infinite loops: if you hit these, an unexpected exception/interrupt happened in M-mode.
# use `info registers` in the qemu monitor to get the relevant CSRs.
unexpected_exception: 
unexpected_interrupt: 
    wfi
    j unexpected_exception

#
# machine-mode timer interrupt.
#
# Sets the next timer and triggers a supervisor software interrupt.
#
.globl m_mode_trap_vector
.align 4
m_mode_trap_vector:
    # start.c has set up the memory that mscratch points to:
    # (indices in xlen_t elements)
    # scratch[0,1,2,3] : register save area.
    # scratch[4] : address of CLINT's MTIMECMP register.
    # scratch[5] : desired interval between interrupts.
    
    # exchange a0 and mscratch and save a1,a2,a3,a4
    csrrw a0, mscratch, a0
    SAVE_REG_TO_SCRATCH(a1, 0)
    SAVE_REG_TO_SCRATCH(a2, 1)
    SAVE_REG_TO_SCRATCH(a3, 2)
    SAVE_REG_TO_SCRATCH(a4, 3)

    csrr a1, mcause
    # we should not get exceptions in M-mode. if we do, something went
    # very wrong and we should explicitly jump to an infinite loop for the
    # purpose.
    bgez a1, unexpected_exception
    li a2, MACHINE_TIMER_INTERRUPT
    # likewise for any interrupts that are not machine timer interrupts.
    bne a1, a2, unexpected_interrupt

    # schedule the next timer interrupt
    # by adding interval to mtimecmp.
    LOAD_REG_FROM_SCRATCH(a1, 4) # load address of CLINT's MTIMECMP register
    LOAD_REG_FROM_SCRATCH(a2, 5) # load interval

    # MTIMECMP += interval;
    # as MTIMECMP is 64 bit on 32 bit systems too, the 32 bit code is more verbose:
#if (__riscv_xlen == 32)
    lw a3, 0(a1)    # load MTIMECMP value lower 32 bit
    lw a4, 4(a1)    # load MTIMECMP value upper 32 bit
    add a3, a3, a2  # MTIMECMP = MTIMECMP + interval
    sltu a2, a3, a2 # if the interval is lower than the sum, set a2 to 1
    add a4, a4, a2  # cause it means an overflow and we need to add 1 to the upper half of the 64 bit value
    sw a3, 0(a1)    # store next interval time lower 32 bit
    sw a4, 4(a1)    # store next interval time upper 32 bit
#else
    ld a3, 0(a1)    # load MTIMECMP value
    add a3, a3, a2  # MTIMECMP += interval
    sd a3, 0(a1)    # store next interval time
#endif

    # arrange for a supervisor software interrupt
    # after this handler returns.
    li a1, 2
    csrw sip, a1

    # restore a0-a4
    LOAD_REG_FROM_SCRATCH(a4, 3)
    LOAD_REG_FROM_SCRATCH(a3, 2)
    LOAD_REG_FROM_SCRATCH(a2, 1)
    LOAD_REG_FROM_SCRATCH(a1, 0)
    csrrw a0, mscratch, a0

    mret
