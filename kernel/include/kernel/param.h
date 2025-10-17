/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/page.h>

///< maximum number of processes, limited by the memory management of
///< the per process kernel stack
#define MAX_PROCESSES 1024
#define MAX_CPUS 8                ///< maximum number of CPUs
#define MAX_FILES_PER_PROCESS 16  ///< open files per process
#define MAX_EXEC_ARGS 32          ///< max exec arguments

/// all stacks start at one page and can grow to this
#define USER_MAX_STACK_SIZE (16 * PAGE_SIZE)

#define KERNEL_STACK_PAGES (1)
#define KERNEL_STACK_SIZE (KERNEL_STACK_PAGES * PAGE_SIZE)

#if defined(DEBUG)
#define CONFIG_DEBUG
#endif  // DEBUG

#ifdef CONFIG_DEBUG
/// adds extra runtime tests to check correct spinlock and rwspinlock
/// usage and stores an optional name per lock for debugging.
#define CONFIG_DEBUG_SPINLOCK

/// adds extra runtime tests to check correct sleeplock
/// usage and stores an optional name per lock for debugging.
#define CONFIG_DEBUG_SLEEPLOCK

/// fill all allocated pages from kalloc with garbage data
/// also fill all freed pages with garbage
// #define CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE

/// some extra API usage tests
#define CONFIG_DEBUG_EXTRA_RUNTIME_TESTS

/// stores the filename / last element of path in an inode for easier debugging
#define CONFIG_DEBUG_INODE_PATH_NAME

#else  // CONFIG_DEBUG
/// allows to query more system info on crashes if NOT set
#ifndef _SHUTDOWN_ON_PANIC
#define _SHUTDOWN_ON_PANIC
#endif  // _SHUTDOWN_ON_PANIC

#endif  // CONFIG_DEBUG

#if defined(CONFIG_DEBUG_EXTRA_RUNTIME_TESTS)
// DEBUG_EXTRA_ASSERT(expected_to_be_true, "message is expectation is broken")
#define DEBUG_EXTRA_ASSERT(test, msg) \
    if (!(test))                      \
    {                                 \
        printk("ERROR: %s\n", msg);   \
    }

// DEBUG_EXTRA_PANIC(expected_to_be_true, "message is expectation is broken")
#define DEBUG_EXTRA_PANIC(test, msg) \
    if (!(test))                     \
    {                                \
        panic(msg);                  \
    }
#else
#define DEBUG_EXTRA_ASSERT(test, msg)
#define DEBUG_EXTRA_PANIC(test, msg)
#endif
