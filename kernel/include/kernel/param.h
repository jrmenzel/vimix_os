/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/page.h>

#define MAX_PROCS 64                 ///< maximum number of processes
#define MAX_CPUS 8                   ///< maximum number of CPUs
#define MAX_FILES_PER_PROCESS 16     ///< open files per process
#define MAX_FILES_SUPPORTED 100      ///< open files per system
#define XV6FS_MAX_ACTIVE_INODES 50   ///< maximum number of active i-nodes
#define MAX_DEVICES 20               ///< maximum major device number
#define MAX_MINOR_DEVICES 4          ///< maximum minor device number
#define MAX_EXEC_ARGS 32             ///< max exec arguments
#define MAX_OP_BLOCKS 10             ///< max # of blocks any FS op writes
#define LOGSIZE (MAX_OP_BLOCKS * 3)  ///< max data blocks in on-disk log
#define NUM_BUFFERS_IN_CACHE (MAX_OP_BLOCKS * 3)  ///< size of disk block cache

/// all stacks start at one page and can grow to this
#define USER_MAX_STACK_SIZE (16 * PAGE_SIZE)

#define KERNEL_STACK_PAGES (1)
#define KERNEL_STACK_SIZE (KERNEL_STACK_PAGES * PAGE_SIZE)

#if defined(DEBUG)
#define CONFIG_DEBUG
#endif  // DEBUG

#ifdef CONFIG_DEBUG
/// adds extra runtime tests to check correct spinlock
/// usage and stores an optional name per lock for debugging.
#define CONFIG_DEBUG_SPINLOCK

/// adds extra runtime tests to check correct sleeplock
/// usage and stores an optional name per lock for debugging.
#define CONFIG_DEBUG_SLEEPLOCK

/// debugs kalloc()
#define CONFIG_DEBUG_KALLOC

/// fill all allocated pages from kalloc with garbage data
/// also fill all freed pages with garbage
#define CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE

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
#define DEBUG_EXTRA_ASSERT(test, msg) \
    if (!(test))                      \
    {                                 \
        printk("ERROR: %s\n", msg);   \
    }
#else
#define DEBUG_EXTRA_ASSERT(test, msg)
#endif
