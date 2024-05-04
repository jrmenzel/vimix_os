/* SPDX-License-Identifier: GPL-2.0 */
/*
 * macros are inspired by the Linux kernel.
 */
#pragma once

#define typeof_member(T, m) typeof(((T *)0)->m)

#ifndef __same_type
#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER)
#endif

/// @short Cast a member of a structure out to the containing structure
/// @param ptr      the pointer to the member.
/// @param type     the type of the container struct this is embedded in.
/// @param member   the name of the member within the struct.
///
/// Just casting pointers (outer_object *d = (outer_object*) contained_object)
/// only works if the contained_object is the first member of outer_object, the
/// macro container_of is more robust and works indepent on where
/// contained_object is located in outer_object.
#define container_of(ptr, type, member)                            \
    ({                                                             \
        void *__mptr = (void *)(ptr);                              \
        _Static_assert(__same_type(*(ptr), ((type *)0)->member) || \
                           __same_type(*(ptr), void),              \
                       "pointer type mismatch in container_of()"); \
        ((type *)(__mptr - offsetof(type, member)));               \
    })
