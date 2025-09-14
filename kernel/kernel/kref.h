/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/stdatomic.h>

/// @brief Simple object for reference counting.
struct kref
{
    atomic_int refcount;
};

/// @brief Init sets the ref count to one.
/// @param kref Reference count object to init.
static inline void kref_init(struct kref *kref)
{
    atomic_store(&kref->refcount, 1);
}

/// @brief Atomically read the current value.
/// @param kref Reference count object to read.
/// @return Current reference count.
static inline unsigned int kref_read(const struct kref *kref)
{
    return atomic_load(&kref->refcount);
}

/// @brief Get a reference -> increase ref count by one.
/// @param kref The reference count object.
static inline void kref_get(struct kref *kref)
{
    atomic_fetch_add_explicit(&kref->refcount, 1, memory_order_relaxed);
}

/// @brief Drop a reference, decrease ref count by one.
/// @param kref The reference count object.
/// @return True if the last reference was dropped and the object should be
/// freed.
static inline bool kref_put(struct kref *kref)
{
    if (atomic_fetch_sub(&kref->refcount, 1) == 1)
    {
        // previous value was 1, now 0 -> free
        return true;
    }
    return false;
}
