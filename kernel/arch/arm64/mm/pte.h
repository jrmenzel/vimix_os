/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/arm64/asm/arm64_def.h>
#include <asm/pgtable-bits.h>
#include <kernel/kernel.h>

static inline pte_t pte_set_executable(pte_t pte) { return pte & ~PTE_XN; }
static inline pte_t pte_unset_executable(pte_t pte) { return pte | PTE_XN; }
#define PTE_IS_EXECUTABLE(pte) (!(pte & PTE_XN))

static inline pte_t pte_set_writeable(pte_t pte) { return pte & ~PTE_RO; }
static inline pte_t pte_unset_writeable(pte_t pte) { return pte | PTE_RO; }
#define PTE_IS_WRITEABLE(pte) (!(pte & PTE_RO))

#define PTE_IS_READABLE(pte) (true)

#define PTE_IS_DIRTY(pte) (pte & PTE_DBM)
#define PTE_WAS_ACCESSED(pte) (pte & PTE_AF)
#define PTE_IS_GLOBAL(pte) (!(pte & PTE_nG))

static inline pte_t pte_clear_user_access(pte_t pte) { return pte & ~PTE_U; }

#define PA2PTE(pa) ((size_t)(pa) & 0x0000FFFFFFFFF000L)
#define PTE_GET_PA(pte) ((size_t)(pte) & 0x0000FFFFFFFFF000L)

#define PTE_FLAGS(pte) ((pte) & (0x600000000003FFL))

// is a non-leaf node valid?
#define PTE_IS_VALID_NODE(pte) (pte & PTE_V)

#define PTE_IS_USER_ACCESSIBLE(pte) (pte & PTE_U)

// is this a valid user accessible leaf?
#define PTE_IS_VALID_USER(pte) \
    (PTE_IS_VALID_NODE(pte) && PTE_IS_USER_ACCESSIBLE(pte))

// On AArch64, valid table descriptors have TYPE=1 but no AF bit, while mapped
// leaf descriptors (page/block) are created with AF set.
#define PTE_IS_LEAF(pte) (PTE_IS_VALID_NODE(pte) && (pte & PTE_AF))

// set valid, unset type
#define PTE_MAKE_VALID_LEAF(pte) (pte = (pte | PTE_V | PTE_TYPE));

// set valid table/page descriptor type for page-table links
#define PTE_MAKE_VALID_TABLE(pte) (pte = (pte | PTE_V | PTE_TYPE));
