/* SPDX-License-Identifier: MIT */
#pragma once

#include <asm/pgtable-bits.h>
#include <kernel/kernel.h>
#include <kernel/page.h>

static inline pte_t pte_set_executable(pte_t pte) { return pte | PTE_X; };
static inline pte_t pte_unset_executable(pte_t pte) { return pte & ~PTE_X; }
#define PTE_IS_EXECUTABLE(pte) (pte & PTE_X)

static inline pte_t pte_set_writeable(pte_t pte) { return pte | PTE_W; }
static inline pte_t pte_unset_writeable(pte_t pte) { return pte & ~PTE_W; }
#define PTE_IS_WRITEABLE(pte) (pte & PTE_W)

#define PTE_IS_READABLE(pte) (pte & PTE_R)

static inline pte_t pte_clear_user_access(pte_t pte) { return pte & ~PTE_U; }

#define PTE_IS_DIRTY(pte) (pte & PTE_D)
#define PTE_WAS_ACCESSED(pte) (pte & PTE_A)
#define PTE_IS_GLOBAL(pte) (pte & PTE_G)

/// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((size_t)pa) >> PAGE_SHIFT) << 10)

#define PTE_GET_PA(pte) (((pte) >> 10) << PAGE_SHIFT)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// is a non-leaf node valid?
#define PTE_IS_VALID_NODE(pte) (pte & PTE_V)

#define PTE_IS_USER_ACCESSIBLE(pte) (pte & PTE_U)

// is this a valid user accessible leaf?
#define PTE_IS_VALID_USER(pte) \
    (PTE_IS_VALID_NODE(pte) && PTE_IS_USER_ACCESSIBLE(pte))

// One of RWX flags -> PTE is a leaf node
#define PTE_IS_LEAF(pte) (pte & (PTE_RW | PTE_X))

#define PTE_MAKE_VALID_LEAF(pte) (pte = pte | PTE_V);

#define PTE_MAKE_VALID_TABLE(pte) (pte = (pte | PTE_V));

/// all mapped paged are ORed with this
#define PTE_MAP_DEFAULT_FLAGS (PTE_V | PTE_A | PTE_D)
