/* SPDX-License-Identifier: MIT */
/// License note: Some defines are taken from GCC's stdatomic.h. If instead I
/// would include that file (which does not include other headers itself), the
/// headers GPL 3 license would not matter due to the GCC Runtime Library
/// Exception. So I assume a manual copy is equivalent. Also I don't know if
/// there are even other ways to define these direct wrappers to compiler
/// built-ins.
/// If these assumptions are wrong, this file will switch to the same GPL 3
/// license.
#pragma once

#include <kernel/types.h>

// also in C11's stdatomic.h:
typedef _Atomic _Bool atomic_bool;
typedef _Atomic char atomic_char;
typedef _Atomic signed char atomic_schar;
typedef _Atomic unsigned char atomic_uchar;
typedef _Atomic short atomic_short;
typedef _Atomic unsigned short atomic_ushort;
typedef _Atomic int atomic_int;
typedef _Atomic unsigned int atomic_uint;
typedef _Atomic long atomic_long;
typedef _Atomic unsigned long atomic_ulong;
typedef _Atomic long long atomic_llong;
typedef _Atomic unsigned long long atomic_ullong;
typedef _Atomic size_t atomic_size_t;
// not in C11's stdatomic.h:
typedef _Atomic ssize_t atomic_ssize_t;
typedef _Atomic uint8_t atomic_uint8_t;
typedef _Atomic uint16_t atomic_uint16_t;
typedef _Atomic uint32_t atomic_uint32_t;
typedef _Atomic uint64_t atomic_uint64_t;
typedef _Atomic int8_t atomic_int8_t;
typedef _Atomic int16_t atomic_int16_t;
typedef _Atomic int32_t atomic_int32_t;
typedef _Atomic int64_t atomic_int64_t;

/// @brief Init an atomic, per C11 standard not atomic itself.
/// @param PTR pointer to atomic variable.
/// @param VAL value to set.
#define atomic_init(PTR, VAL) (*(PTR) = (VAL))

#define __ATOMIC_RELAXED 0
#define __ATOMIC_CONSUME 1
#define __ATOMIC_ACQUIRE 2
#define __ATOMIC_RELEASE 3
#define __ATOMIC_ACQ_REL 4
#define __ATOMIC_SEQ_CST 5

typedef enum memory_order
{
    /// @brief No synchronization or ordering constraints, only atomicity is
    /// guaranteed.
    memory_order_relaxed = __ATOMIC_RELAXED,
    /// @brief Seems to get deprecated in future C standards, avoid using it.
    memory_order_consume = __ATOMIC_CONSUME,
    /// @brief All memory stores before a release in one thread become visible
    /// to all memory loads after an acquire in another thread. Does not affect
    /// other CPUs.
    memory_order_acquire = __ATOMIC_ACQUIRE,
    /// @brief See memory_order_acquire.
    memory_order_release = __ATOMIC_RELEASE,
    /// @brief Combination of acquire and release semantics.
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    /// @brief Full synchronization, "sequentially consistent".
    memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

/// @brief Synchronization fence between threads, often used as memory barrier
/// with mem_order = memory_order_seq_cst.
/// On RISC-V, this emits a fence instruction, on ARM64 a dmb
/// (gcc 15.2 & clang 21.1 via godbolt.org):
///   _relaxed: <nothing generated>    <nothing generated>
///   _consume: fence r,rw             dmb ishld
///   _acquire: fence r,rw             dmb ishld
///   _release: fence rw,w             dmb ish
///   _acq_rel: fence.tso              dmb ish
///   _seq_cst: fence rw,rw            dmb ish
/// @param mem_order The memory order for the fence. memory_order_seq_cst
/// results in a full memory fence on all CPUs.
static inline void atomic_thread_fence(memory_order mem_order)
{
    __atomic_thread_fence(mem_order);
}

/// The _explicit variants below use macros to remove _Atomic and const.
/// The temp vars allow passing any of the supported types and are removed by
/// the compiler. See the GCC stdatomic.h from which they are taken (also see
/// license note above).
///
/// Hints:
/// __extension__({}) prevents warnings about using GCC extensions.
/// __auto_type automatically deduces the type of a variable (*similar* to C++'s
/// auto, but not exactly the same).
/// __typeof__(FOO) returns the type of FOO.
/// __typeof__((void)0, *FOO) removes _Atomic and const from the type of *FOO
/// based on the comma operator ((void)0 is needed to have a comma operator, but
/// gets discarded).

/// @brief Store VAL into the atomic variable pointed to by PTR.
/// @param PTR Pointer to the atomic variable.
/// @param VAL New value to set.
/// @param MO Memory order to use, valid are memory_order_relaxed,
/// memory_order_release and memory_order_seq_cst.
/// On RISC-V, this uses fences.
/// (gcc 15.2 & clang 21.1 via godbolt.org):
///   _relaxed: sb/sh/sw/sd (regular store)
///   _release: fence rw,w + sX
///   _seq_cst: fence rw,w + sX + fence rw,rw
#define atomic_store_explicit(PTR, VAL, MO)                                  \
    __extension__({                                                          \
        __auto_type __atomic_store_ptr = (PTR);                              \
        __typeof__((void)0, *__atomic_store_ptr) __atomic_store_tmp = (VAL); \
        __atomic_store(__atomic_store_ptr, &__atomic_store_tmp, (MO));       \
    })

/// @brief Store VAL into the atomic variable pointed to by PTR.
/// Uses memory_order_seq_cst.
/// @param PTR Pointer to the atomic variable.
/// @param VAL New value to set.
#define atomic_store(PTR, VAL) atomic_store_explicit(PTR, VAL, __ATOMIC_SEQ_CST)

/// @brief Load the value of the atomic variable pointed to by PTR.
/// @param PTR Pointer to the atomic variable.
/// @param MO Memory order to use, valid are memory_order_relaxed,
/// memory_order_consume, memory_order_aquire and memory_order_seq_cst.
/// On RISC-V, this uses fences.
/// (gcc 15.2 & clang 21.1 via godbolt.org):
///   _relaxed: lb/lh/lw/ld (regular load)
///   _consume: lX + fence r,rw
///   _acquire: lX + fence r,rw
///   _seq_cst: fence rw,rw + lX + fence r,rw
/// @return The loaded value.
#define atomic_load_explicit(PTR, MO)                               \
    __extension__({                                                 \
        __auto_type __atomic_load_ptr = (PTR);                      \
        __typeof__((void)0, *__atomic_load_ptr) __atomic_load_tmp;  \
        __atomic_load(__atomic_load_ptr, &__atomic_load_tmp, (MO)); \
        __atomic_load_tmp;                                          \
    })

/// @brief Load the value of the atomic variable pointed to by PTR.
/// Uses memory_order_seq_cst.
/// @param PTR Pointer to the atomic variable.
/// @return The loaded value.
#define atomic_load(PTR) atomic_load_explicit(PTR, __ATOMIC_SEQ_CST)

/// @brief Atomically replace the value pointed to by PTR with VAL and return
/// the old value.
/// On RISC-V, this emits a amoswap instruction.
/// (gcc 15.2 & clang 21.1 via godbolt.org):
///   _relaxed: amoswap.w
///   _consume: amoswap.w.aq
///   _acquire: amoswap.w.aq
///   _release: amoswap.w.rl
///   _acq_rel: amoswap.w.aqrl
///   _seq_cst: amoswap.w.aqrl
/// @param PTR Pointer to the atomic variable.
/// @param VAL New value to set.
/// @param MO Memory order to use.
/// @return The old value.
#define atomic_exchange_explicit(PTR, VAL, MO)                              \
    __extension__({                                                         \
        __auto_type __atomic_exchange_ptr = (PTR);                          \
        __typeof__((void)0, *__atomic_exchange_ptr) __atomic_exchange_val = \
            (VAL);                                                          \
        __typeof__((void)0, *__atomic_exchange_ptr) __atomic_exchange_tmp;  \
        __atomic_exchange(__atomic_exchange_ptr, &__atomic_exchange_val,    \
                          &__atomic_exchange_tmp, (MO));                    \
        __atomic_exchange_tmp;                                              \
    })

/// @brief Atomically replace the value pointed to by PTR with VAL and return
/// the old value. Uses memory_order_seq_cst.
/// @param PTR Pointer to the atomic variable.
/// @param VAL New value to set.
/// @return The old value.
#define atomic_exchange(PTR, VAL) \
    atomic_exchange_explicit(PTR, VAL, __ATOMIC_SEQ_CST)

/// @brief Atomically compare the value pointed to by PTR with *VAL. If equal,
/// replace it with *DES and return true. Otherwise, load the actual value into
/// *VAL and return false. Pseudo code (if atomic):
///
/// if (*PTR == *VAL) {
///     *PTR = *DES;
///     return true;
/// } else {
///     *VAL = *PTR;
///     return false;
/// }
///
/// On RISC-V, this uses lr.X (load reserved)/sc.X (store conditional)
/// instructions.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Pointer to the expected value, updated with the actual value if
/// the comparison fails.
/// @param DES Pointer to the desired value to set if the comparison succeeds.
/// @param SUC Memory order to use for the read-modify-write operation if *PTR
/// == *VAL.
/// @param FAIL Memory order to use for the read operation if *PTR != *VAL.
/// @return true if the comparison succeeded and the value was replaced,
/// false otherwise.
#define atomic_compare_exchange_strong_explicit(PTR, VAL, DES, SUC, FAIL)   \
    __extension__({                                                         \
        __auto_type __atomic_compare_exchange_ptr = (PTR);                  \
        __typeof__((void)0, *__atomic_compare_exchange_ptr)                 \
            __atomic_compare_exchange_tmp = (DES);                          \
        __atomic_compare_exchange(__atomic_compare_exchange_ptr, (VAL),     \
                                  &__atomic_compare_exchange_tmp, 0, (SUC), \
                                  (FAIL));                                  \
    })

/// @brief Atomically compare the value pointed to by PTR with *VAL. If equal,
/// replace it with *DES and return true. Otherwise, load the actual value into
/// *VAL and return false. Pseudo code (if atomic):
///
/// if (*PTR == *VAL) {
///     *PTR = *DES;
///     return true;
/// } else {
///     *VAL = *PTR;
///     return false;
/// }
///
/// On RISC-V, this uses lr.X (load reserved)/sc.X (store conditional)
/// instructions.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Pointer to the expected value, updated with the actual value if
/// the comparison fails.
/// @param DES Pointer to the desired value to set if the comparison succeeds.
/// @return true if the comparison succeeded and the value was replaced,
/// false otherwise.
#define atomic_compare_exchange_strong(PTR, VAL, DES)                        \
    atomic_compare_exchange_strong_explicit(PTR, VAL, DES, __ATOMIC_SEQ_CST, \
                                            __ATOMIC_SEQ_CST)

/// @brief Like atomic_compare_exchange_strong_explicit, but may fail
/// spuriously. If used in a loop anyways, it can be faster though.
#define atomic_compare_exchange_weak_explicit(PTR, VAL, DES, SUC, FAIL)     \
    __extension__({                                                         \
        __auto_type __atomic_compare_exchange_ptr = (PTR);                  \
        __typeof__((void)0, *__atomic_compare_exchange_ptr)                 \
            __atomic_compare_exchange_tmp = (DES);                          \
        __atomic_compare_exchange(__atomic_compare_exchange_ptr, (VAL),     \
                                  &__atomic_compare_exchange_tmp, 1, (SUC), \
                                  (FAIL));                                  \
    })

/// @brief Like atomic_compare_exchange_strong, but may fail spuriously. If used
/// in a loop anyways, it can be faster though.
#define atomic_compare_exchange_weak(PTR, VAL, DES)                        \
    atomic_compare_exchange_weak_explicit(PTR, VAL, DES, __ATOMIC_SEQ_CST, \
                                          __ATOMIC_SEQ_CST)

/// @brief Atomically performs (*PTR += VAL) and returns the old value.
/// Uses memory_order_seq_cst.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Value to add.
/// @return The old value.
#define atomic_fetch_add(PTR, VAL) \
    __atomic_fetch_add((PTR), (VAL), __ATOMIC_SEQ_CST)

/// @brief Atomically performs (*PTR += VAL) and returns the old value.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Value to add.
/// @param MO Memory order to use.
/// On RISC-V, this emits a amoadd instruction.
/// (gcc 15.2 & clang 21.1 via godbolt.org):
///   _relaxed: amoadd.w
///   _consume: amoadd.w.aq
///   _acquire: amoadd.w.aq
///   _release: amoadd.w.rl
///   _acq_rel: amoadd.w.aqrl
///   _seq_cst: amoadd.w.aqrl
/// @return The old value.
#define atomic_fetch_add_explicit(PTR, VAL, MO) \
    __atomic_fetch_add((PTR), (VAL), (MO))

/// @brief Atomically performs (*PTR -= VAL) and returns the old value.
/// Uses memory_order_seq_cst.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Value to subtract.
/// @return The old value.
#define atomic_fetch_sub(PTR, VAL) \
    __atomic_fetch_sub((PTR), (VAL), __ATOMIC_SEQ_CST)

/// @brief Atomically performs (*PTR -= VAL) and returns the old value.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Value to subtract.
/// @param MO Memory order to use.
/// On RISC-V, this emits a amoadd instruction with -VAL (there is no amosub).
/// (gcc 15.2 & clang 21.1 via godbolt.org):
///   _relaxed: amoadd.w
///   _consume: amoadd.w.aq
///   _acquire: amoadd.w.aq
///   _release: amoadd.w.rl
///   _acq_rel: amoadd.w.aqrl
///   _seq_cst: amoadd.w.aqrl
/// @return The old value.
#define atomic_fetch_sub_explicit(PTR, VAL, MO) \
    __atomic_fetch_sub((PTR), (VAL), (MO))

/// @brief Atomically performs (*PTR |= VAL) and returns the old value.
/// Uses memory_order_seq_cst.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Value to subtract.
/// @return The old value.
#define atomic_fetch_or(PTR, VAL) \
    __atomic_fetch_or((PTR), (VAL), __ATOMIC_SEQ_CST)

/// @brief Atomically performs (*PTR |= VAL) and returns the old value.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Value to OR.
/// @param MO Memory order to use.
/// On RISC-V, this emits a amoor instruction.
/// (gcc 15.2 & clang 21.1 via godbolt.org):
///   _relaxed: amoor.w
///   _consume: amoor.w.aq
///   _acquire: amoor.w.aq
///   _release: amoor.w.rl
///   _acq_rel: amoor.w.aqrl
///   _seq_cst: amoor.w.aqrl
/// @return The old value.
#define atomic_fetch_or_explicit(PTR, VAL, MO) \
    __atomic_fetch_or((PTR), (VAL), (MO))

/// @brief Atomically performs (*PTR ^= VAL) and returns the old value.
/// Uses memory_order_seq_cst.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Value to XOR.
/// @return The old value.
#define atomic_fetch_xor(PTR, VAL) \
    __atomic_fetch_xor((PTR), (VAL), __ATOMIC_SEQ_CST)

/// @brief Atomically performs (*PTR ^= VAL) and returns the old value.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Value to XOR.
/// On RISC-V, this emits a amoor instruction.
/// (gcc 15.2 & clang 21.1 via godbolt.org):
///   _relaxed: amoxor.w
///   _consume: amoxor.w.aq
///   _acquire: amoxor.w.aq
///   _release: amoxor.w.rl
///   _acq_rel: amoxor.w.aqrl
///   _seq_cst: amoxor.w.aqrl
/// @return The old value.
#define atomic_fetch_xor_explicit(PTR, VAL, MO) \
    __atomic_fetch_xor((PTR), (VAL), (MO))

/// @brief Atomically performs (*PTR &= VAL) and returns the old value.
/// Uses memory_order_seq_cst.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Value to AND.
/// @return The old value.
#define atomic_fetch_and(PTR, VAL) \
    __atomic_fetch_and((PTR), (VAL), __ATOMIC_SEQ_CST)

/// @brief Atomically performs (*PTR &= VAL) and returns the old value.
/// @param PTR Pointer to the atomic variable.
/// @param VAL Value to AND.
/// On RISC-V, this emits a amoor instruction.
/// (gcc 15.2 & clang 21.1 via godbolt.org):
///   _relaxed: amoand.w
///   _consume: amoand.w.aq
///   _acquire: amoand.w.aq
///   _release: amoand.w.rl
///   _acq_rel: amoand.w.aqrl
///   _seq_cst: amoand.w.aqrl
/// @return The old value.
#define atomic_fetch_and_explicit(PTR, VAL, MO) \
    __atomic_fetch_and((PTR), (VAL), (MO))
