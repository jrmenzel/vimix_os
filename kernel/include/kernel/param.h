/* SPDX-License-Identifier: MIT */
#pragma once

#define MAX_PROCS 64                 ///< maximum number of processes
#define MAX_CPUS 8                   ///< maximum number of CPUs
#define MAX_FILES_PER_PROCESS 16     ///< open files per process
#define MAX_FILES_SUPPORTED 100      ///< open files per system
#define MAX_ACTIVE_INODES 50         ///< maximum number of active i-nodes
#define MAX_DEVICES 10               ///< maximum major device number
#define ROOT_DEVICE_NUMBER 1         ///< device number of file system root disk
#define MAX_EXEC_ARGS 32             ///< max exec arguments
#define MAX_OP_BLOCKS 10             ///< max # of blocks any FS op writes
#define LOGSIZE (MAX_OP_BLOCKS * 3)  ///< max data blocks in on-disk log
#define NUM_BUFFERS_IN_CACHE (MAX_OP_BLOCKS * 3)  ///< size of disk block cache
#define FSSIZE 2000   ///< size of file system in blocks
#define PATH_MAX 128  ///< maximum file path name
