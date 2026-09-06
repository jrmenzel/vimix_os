#!/usr/bin/env python3
"""Exercise the ARM trap context with a GIC whose next interrupt can change."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]

# Mock only hardware access and kernel dependencies. Compile the production
# context creation, classification, and completion code on the host.
STUBS = {
    "kernel/kernel.h": """
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#define DEBUG_EXTRA_PANIC(condition, message) assert(condition)
""",
    "arch/barrier.h": """
#pragma once
#define isb() ((void)0)
#define dsb(scope) ((void)0)
""",
    "arch/arm64/drivers/gic_v2.h": """
#pragma once
#include <stdint.h>
uint32_t gic2_acknowledge(void);
void gic2_end_interrupt(uint32_t token);
""",
    "arch/arm64/arm64.h": """
#pragma once
#include <kernel/kernel.h>
#define ESR_GET_EXC_CLASS(esr) (esr)
#define ESR_EC_SVC_A64 0x15
#define ESR_EC_INSN_ABORT_EL0 0x20
#define ESR_EC_INSN_ABORT_EL1 0x21
#define ESR_EC_DATA_ABORT_EL0 0x24
#define ESR_EC_DATA_ABORT_EL1 0x25
static inline size_t arm_read_esr_el1(void) { return 0; }
static inline size_t arm_read_elr_el1(void) { return 1; }
static inline size_t arm_read_spsr_el1(void) { return 0; }
static inline size_t arm_read_far_el1(void) { return 0; }
static inline size_t arm_read_cntv_ctl_el0(void) { return 5; }
static inline void arm_write_elr_el1(size_t value) { (void)value; }
static inline void arm_write_spsr_el1(size_t value) { (void)value; }
""",
}

TEST = r"""
#include <arch/interrupts.h>
#include <stdio.h>

static uint32_t next_token, completed_token;
static unsigned claims, completions;

uint32_t gic2_acknowledge(void)
{
    ++claims;
    return next_token;
}

void gic2_end_interrupt(uint32_t token)
{
    ++completions;
    completed_token = token;
}

int main(void)
{
    struct Interrupt_Context ctx, nested;

    // A device is claimed, then a timer arrives before device dispatch.
    // Dispatch and completion must still refer to the claimed device.
    next_token = 48;
    int_ctx_create(&ctx, CTX_CLASS_CURRENT_EL_SP_ELX, CTX_TYPE_IRQ);
    next_token = 27;
    assert(int_ctx_source_is_device(&ctx));
    assert(!int_ctx_source_is_timer(&ctx));
    assert(!int_ctx_source_is_ipi(&ctx));
    assert(int_ctx_claim_device(&ctx) == 48);
    int_ctx_complete_device(&ctx, 48);
    assert(claims == 1 && completions == 0);
    int_ctx_complete(&ctx);
    assert(completions == 1 && completed_token == 48);

    // Conversely, a timer remains a timer even if the next IRQ is a device.
    int_ctx_create(&ctx, CTX_CLASS_LOWER_EL_AARCH64, CTX_TYPE_IRQ);
    next_token = 48;
    assert(int_ctx_source_is_timer(&ctx));
    int_acknowledge_timer();
    assert(claims == 2);
    int_ctx_complete(&ctx);
    assert(completed_token == 27);

    // Nested contexts must preserve the SGI source CPU in the full IAR token.
    next_token = (3u << 10) | ARM64_IPI_SGI_ID;
    int_ctx_create(&ctx, CTX_CLASS_CURRENT_EL_SP_ELX, CTX_TYPE_FIQ);
    assert(int_ctx_source_is_ipi(&ctx));
    int_acknowledge_ipi();
    next_token = 27;
    int_ctx_create(&nested, CTX_CLASS_CURRENT_EL_SP_ELX, CTX_TYPE_IRQ);
    int_ctx_complete(&nested);
    assert(completed_token == 27);
    int_ctx_complete(&ctx);
    assert(completed_token == ((3u << 10) | ARM64_IPI_SGI_ID));
    assert(claims == 4 && completions == 4);
    int_ctx_complete(&ctx);
    assert(completions == 4);

    // No EOI for special IDs, even with the timer status reporting expired.
    for (next_token = 1020; next_token <= 1023; ++next_token)
    {
        int_ctx_create(&ctx, CTX_CLASS_CURRENT_EL_SP_ELX, CTX_TYPE_IRQ);
        assert(int_ctx_source_is_spurious(&ctx));
        assert(!int_ctx_source_is_timer(&ctx));
        assert(!int_ctx_source_is_ipi(&ctx));
        assert(!int_ctx_source_is_device(&ctx));
        int_ctx_complete(&ctx);
    }
    assert(claims == 8 && completions == 4);

    // An exception must not consume an interrupt awaiting delivery.
    next_token = 27;
    int_ctx_create(&ctx, CTX_CLASS_LOWER_EL_AARCH64, CTX_TYPE_SYNCHRONOUS);
    int_ctx_complete(&ctx);
    int_ctx_create(&ctx, CTX_CLASS_CURRENT_EL_SP_ELX, CTX_TYPE_SERROR);
    int_ctx_complete(&ctx);
    assert(claims == 8 && completions == 4);
    puts("PASS: ARM64 interrupt dispatch and acknowledgement ownership");
}
"""


def main():
    with tempfile.TemporaryDirectory(prefix="vimixos-irq-test-") as directory:
        tmp = Path(directory)
        for name, contents in STUBS.items():
            path = tmp / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents)
        source = tmp / "test.c"
        source.write_text(TEST)
        binary = tmp / "test"
        subprocess.run(
            [
                "cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                "-Wno-unused-parameter",
                "-I", str(tmp),
                "-I", str(ROOT / "kernel/arch/arm64"),
                "-I", str(ROOT / "kernel"),
                str(source), str(ROOT / "kernel/arch/arm64/arch_interrupts.c"),
                "-o", str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
