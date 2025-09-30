/* SPDX-License-Identifier: MIT */

#include <arch/riscv/asm/m_mode.h>
#include <arch/riscv/asm/registers.h>
#include <arch/riscv/sbi_defs.h>
#include <arch/timer.h>
#include <kernel/cpu.h>
#include <kernel/page.h>
#include <kernel/param.h>

__attribute__((aligned(M_MODE_STACK))) __attribute__((
    section("STACK"))) char g_m_mode_cpu_stack[MAX_CPUS * M_MODE_STACK];

struct m_code_cpu_data
{
    size_t start_addr;
    size_t opaque;
    size_t int_cause;
    size_t hart_status;
};

__attribute__((
    section("M_MODE"))) struct m_code_cpu_data g_m_mode_cpu_data[MAX_CPUS];

// atomically read when performing the lottery to get the boot hart, so keep 32
// bit
__attribute__((section("M_MODE"))) int32_t g_m_mode_boot_hart = MAX_CPUS;

extern void m_mode_trap_vector();
extern void _entry_s_mode();

/// enable machine mode interrupts by setting bit MSTATUS_MIE in mstatus
static inline void cpu_enable_m_mode_interrupts()
{
    rv_write_csr_mstatus(rv_read_csr_mstatus() | MSTATUS_MIE | MSTATUS_MPIE);
    rv_write_csr_mie(rv_read_csr_mie() | MIE_MSIE);
}

/// enable machine mode timer interrupts
static inline void cpu_enable_m_mode_timer_interrupt()
{
    rv_write_csr_mie(rv_read_csr_mie() | MIE_MTIE);
}

/// disable machine mode timer interrupts
static inline void cpu_disable_m_mode_timer_interrupt()
{
    rv_write_csr_mie(rv_read_csr_mie() & ~MIE_MTIE);
}

/// Set the Machine-mode trap vector (interrupt handler) function
static inline void cpu_set_m_mode_trap_vector(void *machine_mode_trap_vector)
{
    rv_write_csr_mtvec((xlen_t)machine_mode_trap_vector);
}

const size_t CLINT_BASE = 0x02000000L;

/// Compare value for the timer. Always 64 bit!
#define CLINT_MTIMECMP(hartid) \
    (*(uint64_t *)(CLINT_BASE + 0x4000 + 8 * (hartid)))

/// Cycles since boot. This register is always 64 bit!
#define CLINT_MTIME (*(uint64_t *)(CLINT_BASE + 0xBFF8))

#define CLINT_MSIP(hartid) (*(uint32_t *)(CLINT_BASE + 4 * (hartid)))

static inline void clint_set_timer(uint64_t time)
{
    xlen_t id = rv_read_csr_mhartid();
    // 64bit time even on 32bit systems!
    CLINT_MTIMECMP(id) = time;
}

// setup memory protection and interrupts, run for all harts
void m_mode_start(size_t hart_id)
{
    g_m_mode_cpu_data[hart_id].start_addr = 0;
    g_m_mode_cpu_data[hart_id].opaque = 0;
    g_m_mode_cpu_data[hart_id].int_cause = INT_CAUSE_NONE;
    g_m_mode_cpu_data[hart_id].hart_status = SBI_HSM_HART_STOPPED;

    // delegate all interrupts and exceptions to supervisor mode except
    // ecalls from the kernel and illegal instructions
    rv_write_csr_medeleg(MEDLEG_ALL & ~(MEDLELEG_ECALL_FROM_S_MODE |
                                        MEDLELEG_ILLEGAL_INSTRUCTION));
    rv_write_csr_mideleg(0xffff);

    // configure Physical Memory Protection to give supervisor mode
    // access to all of physical memory.
    rv_write_csr_pmpaddr0(PMP_RANGE_TOP);
    rv_write_csr_pmpaddr1(PMP_RANGE_BOTTOM);
    rv_write_csr_pmpcfg0(PMP_R | PMP_W | PMP_X | PMP_MATCH_NAPOT);

    // allow supervisor to use stimecmp and time.
    rv_write_csr_mcounteren(rv_read_csr_mcounteren() | 2);

#if defined(__RISCV_EXT_SSTC)
    // enable the sstc extension (i.e. csr stimecmp)
    // the menvcfg csr is 64 bit (even on 32 bit systems)
    // enable sstc by setting bit 63
#if defined(__ARCH_32BIT)
    rv_write_csr_menvcfgh(rv_read_csr_menvcfgh() | HIGHEST_BIT);
#else
    rv_write_csr_menvcfg(rv_read_csr_menvcfg() | HIGHEST_BIT);
#endif

    // enable supervisor-mode timer interrupts.
    rv_write_csr_mie(rv_read_csr_mie() | MIE_STIE);
#endif  // __RISCV_EXT_SSTC

    // set interrupt handler and enable interrupts
    cpu_set_m_mode_trap_vector(m_mode_trap_vector);
    cpu_enable_m_mode_interrupts();

    atomic_thread_fence(memory_order_seq_cst);
}

// only the boot hart runs this
void m_mode_boot_hart_setup(size_t hart_id, size_t dtb)
{
    g_m_mode_cpu_data[hart_id].start_addr = (size_t)_entry_s_mode;
    g_m_mode_cpu_data[hart_id].opaque = dtb;
    g_m_mode_cpu_data[hart_id].hart_status = SBI_HSM_HART_STARTED;
    atomic_thread_fence(memory_order_seq_cst);
}

struct ret_value
{
    size_t a0;
    size_t a1;
};

struct ret_value m_mode_prepare_start_hart(xlen_t *stack)
{
    size_t hart_id = rv_read_csr_mhartid();
    // 1. set M Exception Program Counter to given function, so mret will jump
    // there
    //    (requires gcc -mcmodel=medany)
    rv_write_csr_mepc((xlen_t)g_m_mode_cpu_data[hart_id].start_addr);

    // 2. set M Previous Privilege mode to Supervisor, so mret will switch to S
    // mode
    xlen_t machine_status = rv_read_csr_mstatus();
    machine_status &= ~MSTATUS_MPP_MASK;
    machine_status |= MSTATUS_MPP_S;
    rv_write_csr_mstatus(machine_status);

    if (stack != NULL)
    {
        // return a0/a1 on the stack for booting additional cores
        stack[IDX_ALL_A0] = hart_id;
        stack[IDX_ALL_A1] = g_m_mode_cpu_data[hart_id].opaque;
    }

    g_m_mode_cpu_data[hart_id].hart_status = SBI_HSM_HART_STARTED;

    // return values will end up in a0 / a1 which will become the parameters of
    // the called function (needed for the first hart to boot)
    struct ret_value ret;
    ret.a0 = hart_id;
    ret.a1 = g_m_mode_cpu_data[hart_id].opaque;
    return ret;
}

struct sbiret m_mode_handle_sbi_call(xlen_t arg0, xlen_t arg1, xlen_t arg2,
                                     xlen_t arg3, xlen_t arg4, xlen_t mcause,
                                     xlen_t fid, xlen_t ext)
{
    struct sbiret ret;
    ret.error = SBI_ERR_NOT_SUPPORTED;
    ret.value = 0;

    if (ext == SBI_EXT_ID_BASE)
    {
        // get the spec version or test for extensions
        if (fid == SBI_BASE_GET_SPEC_VERSION)
        {
            ret.error = SBI_SUCCESS;
            ret.value = (2 << SBI_SPEC_VERSION_MAJOR_SHIFT) | (0);
        }
        else if (fid == SBI_BASE_PROBE_EXTENSION)
        {
            ret.error = SBI_SUCCESS;
            size_t ext_id = arg0;
            if ((ext_id == SBI_EXT_ID_TIME) || (ext_id == SBI_EXT_ID_HSM))
            {
                ret.value = 1;
            }
        }
        else if (fid == SBI_BASE_GET_IMPL_ID)
        {
            ret.error = SBI_SUCCESS;
            ret.value = SBI_IMPL_ID_VIMIX;
        }
        else if (fid == SBI_BASE_GET_IMPL_VERSION)
        {
            ret.error = SBI_SUCCESS;
            ret.value = 1;
        }
    }
    else if (ext == SBI_EXT_ID_TIME)
    {
        // timer extension
        if (fid == SBI_TIME_SET_TIMER)
        {
            // set timer at arg0
            clint_set_timer(arg0);
            cpu_enable_m_mode_timer_interrupt();

            ret.error = SBI_SUCCESS;
        }
    }
    else if (ext == SBI_EXT_ID_HSM)
    {
        // start other harts, not spec complient, but does what VIMIX expects
        if (fid == SBI_HSM_HART_START)
        {
            size_t hart_id = arg0;
            size_t start_addr = arg1;

            if (hart_id > MAX_CPUS)
            {
                ret.error = SBI_ERR_INVALID_PARAM;
                return ret;
            }
            if (g_m_mode_cpu_data[hart_id].start_addr != 0)
            {
                ret.error = SBI_ERR_ALREADY_AVAILABLE;
                return ret;
            }
            if (start_addr == 0)
            {
                ret.error = SBI_ERR_INVALID_ADDRESS;
                return ret;
            }
            g_m_mode_cpu_data[hart_id].start_addr = start_addr;
            g_m_mode_cpu_data[hart_id].opaque = arg2;
            g_m_mode_cpu_data[hart_id].int_cause = INT_CAUSE_START;
            g_m_mode_cpu_data[hart_id].hart_status = SBI_HSM_HART_START_PENDING;
            atomic_thread_fence(memory_order_seq_cst);
            ret.error = SBI_SUCCESS;

            // Trigger software interrupt on remote hart
            CLINT_MSIP(hart_id) = 1;
        }
        else if (fid == SBI_HSM_HART_STATUS)
        {
            atomic_thread_fence(memory_order_seq_cst);
            ret.value = g_m_mode_cpu_data[arg0].hart_status;
            ret.error = SBI_SUCCESS;
        }
    }
    else if (ext == SBI_EXT_ID_IPI)
    {
        if (fid == SBI_IPI_SEND_IPI)
        {
            size_t hart_mask = arg0;
            size_t hart_base = arg1;
            for (size_t i = 0; i < sizeof(size_t) * 8; i++)
            {
                if (hart_mask & ((size_t)1 << i))
                {
                    size_t hart = hart_base + i;
                    if (hart >= MAX_CPUS)
                    {
                        ret.error = SBI_ERR_INVALID_PARAM;
                        return ret;
                    }
                    CLINT_MSIP(hart) = 1;
                }
            }
            ret.error = SBI_SUCCESS;
        }
    }
    return ret;
}

void prepare_return_to_next_instruction()
{
    xlen_t mepc = rv_read_csr_mepc();
    mepc += 4;  // assume 32 bit instructions, which is the case for csr reads
                // and ecalls
    rv_write_csr_mepc(mepc);
}

bool m_mode_handle_illegal_instruction(xlen_t *stack)
{
    uint32_t inst = (uint32_t)rv_read_csr_mtval();
    // test for csrr, which is encoded as csrrs rd, csr, x0
    const uint32_t rs1_mask = (0x1F << 15);
    const uint32_t funct3_mask = (0x7 << 12);
    const uint32_t opcode_mask = 0x7F;
    const uint32_t rd_mask = (0x1F << 7);
    const uint32_t csr_mask = (0xFFF << 20);
    const uint32_t csrr_inst_mask = (rs1_mask | funct3_mask | opcode_mask);
    const uint32_t funct3_csrrs = (2 << 12);
    const uint32_t opcode_system = 0x73;
    const uint32_t csrr_inst_values = (funct3_csrrs | opcode_system);

    if ((inst & csrr_inst_mask) == csrr_inst_values)
    {
        // found a csrrs rd, csr, x0 instruction
        const uint32_t csr_time = 0xC01;
        const uint32_t csr_timeh = 0xC81;

        int32_t csr = (inst & csr_mask) >> 20;
        if ((csr == csr_time) || (csr == csr_timeh))
        {
            uint32_t rd = (inst & rd_mask) >> 7;
            uint64_t time = CLINT_MTIME;
#if (__riscv_xlen == 32)
            if (csr == csr_timeh)
            {
                time = time >> 32;
            }
#endif
            stack[rd - 1] = (xlen_t)time;
            prepare_return_to_next_instruction();
            return true;
        }
    }
    return false;
}

void m_mode_interrupt_handler(xlen_t *stack)
{
    xlen_t mcause = rv_read_csr_mcause();

    if (mcause == MCAUSE_ECALL_FROM_S_MODE)
    {
        struct sbiret ret = m_mode_handle_sbi_call(
            stack[IDX_ALL_A0], stack[IDX_ALL_A1], stack[IDX_ALL_A2],
            stack[IDX_ALL_A3], stack[IDX_ALL_A4], stack[IDX_ALL_A5],
            stack[IDX_ALL_A6], stack[IDX_ALL_A7]);
        stack[IDX_ALL_A0] = ret.error;
        stack[IDX_ALL_A1] = ret.value;
        prepare_return_to_next_instruction();
    }
    else if (mcause == MCAUSE_MACHINE_TIMER)
    {
        clint_set_timer((-1));  // infinitely far into the future

        // arrange for a supervisor software interrupt
        // after this handler returns so S Mode gets a timer interrupt
        rv_write_csr_sip(rv_read_csr_sip() | SIP_SSIP);
    }
    else if (mcause == MCAUSE_MACHINE_SOFTWARE)
    {
        size_t hart_id = rv_read_csr_mhartid();
        CLINT_MSIP(hart_id) = 0;  // clear int

        if (g_m_mode_cpu_data[hart_id].int_cause == INT_CAUSE_START)
        {
            g_m_mode_cpu_data[hart_id].int_cause = INT_CAUSE_NONE;
            m_mode_prepare_start_hart(stack);
        }
        else
        {
            // arrange for a supervisor software interrupt
            // after this handler returns so S Mode gets an IPI
            rv_write_csr_sip(rv_read_csr_sip() | SIP_SSIP);
        }
    }
    else if (mcause == MCAUSE_ILLEGAL_INSTRUCTION)
    {
        m_mode_handle_illegal_instruction(stack);
    }
}
