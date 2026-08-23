/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/interrupts.h>
#include <init/dtb.h>
#include <init/main.h>
#include <init/start.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/page.h>
#include <kernel/timer.h>

void cpu_set_boot_state()
{
    rv_write_csr_sie(0);
    cpu_disable_interrupts();
}

// atomically read when performing the lottery to get the hart preparing the
// initial page table, so keep 32 bit
int32_t g_early_pt_hart = 1;

// signal that the early page table is ready when set to 0.
// starting at 1 to keep out of BSS.
int32_t g_early_pt_done = 1;

/// @brief Checks if an extension is part of the riscv_isa string
/// @param riscv_isa E.g. "rv64imafdc_zicsr_sstc"
/// @param ext
/// @return
bool extension_is_supported(const char *riscv_isa, const char *ext)
{
    riscv_isa += 4;  // first 4 chars are "rv32" or "rv64"
    // one char extensions are combined in the begining of the string:
    size_t ext_len = strlen(ext);
    if (ext_len == 1)
    {
        while (*riscv_isa != '_' && *riscv_isa != 0)
        {
            if (*riscv_isa == ext[0])
            {
                return true;
            }
            riscv_isa++;
        }
    }
    else
    {
        char *pos;
        while ((pos = strstr(riscv_isa, ext)) != NULL)
        {
            // potential match
            pos -= 1;  // move pointer back (there is always a valid char as
                       // riscv_isa was moved forward before)

            // found location does not have a '_' before it: found a match
            // inside of another ext ("ext" in "rv64imac_newext_foo")
            if (*pos != '_') continue;

            // also check end of _ext_ substring: can be '_' or end of
            // string!
            if (pos[ext_len + 1] != '_' && pos[ext_len + 2] != 0) continue;

            return true;
        }
    }
    return false;
}

void dtb_get_cpu_features(const void *dtb, size_t cpu_id,
                          CPU_Features *features_out)
{
    CPU_Features featues = 0;

    int offset = dtb_get_cpu_offset(dtb, cpu_id, true);
    if (offset < 0) return;

    // parse MMU support
    int mmu_type_len;
    const char *mmu_type = fdt_getprop(dtb, offset, "mmu-type", &mmu_type_len);
    if (mmu_type != NULL)
    {
        if (strcmp(mmu_type, "riscv,sv32") == 0)
        {
            featues |= RV_SV32_SUPPORTED;
        }
        else if (strcmp(mmu_type, "riscv,sv39") == 0)
        {
            featues |= RV_SV39_SUPPORTED;
        }
        else if (strcmp(mmu_type, "riscv,sv48") == 0)
        {
            featues |= RV_SV48_SUPPORTED;
        }
        else if (strcmp(mmu_type, "riscv,sv57") == 0)
        {
            featues |= RV_SV57_SUPPORTED;
        }
    }

    // potentially relevant extensions
    int riscv_isa_len;
    const char *riscv_isa =
        fdt_getprop(dtb, offset, "riscv,isa", &riscv_isa_len);
    if (riscv_isa != NULL)
    {
#if defined(__RISCV_EXT_SSTC)
        if (extension_is_supported(riscv_isa, "sstc"))
        {
            featues |= RV_EXT_SSTC;
        }
#endif
        if (extension_is_supported(riscv_isa, "f"))
        {
            featues |= RV_EXT_FLOAT;
        }
        if (extension_is_supported(riscv_isa, "d"))
        {
            featues |= RV_EXT_DOUBLE;
        }
    }

    int riscv_isa_ext_len;
    const char *riscv_isa_ext =
        fdt_getprop(dtb, offset, "riscv,isa-extensions", &riscv_isa_ext_len);
    if (riscv_isa_ext != NULL)
    {
        while (true)
        {
#if defined(__RISCV_EXT_SSTC)
            if (strcmp(riscv_isa_ext, "sstc"))
            {
                featues |= RV_EXT_SSTC;
            }
#endif
            if (strcmp(riscv_isa_ext, "f"))
            {
                featues |= RV_EXT_FLOAT;
            }
            if (strcmp(riscv_isa_ext, "d"))
            {
                featues |= RV_EXT_DOUBLE;
            }

            riscv_isa_ext += strlen(riscv_isa_ext) + 1;
            if (riscv_isa_ext[0] == 0) break;
        }
    }

    *features_out = featues;
}
