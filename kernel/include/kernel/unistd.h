/* SPDX-License-Identifier: MIT */
#pragma once

// System call numbers
#define SYS_fork 1
#define SYS_exit 2
#define SYS_wait 3
#define SYS_pipe 4
#define SYS_read 5
#define SYS_kill 6
#define SYS_execv 7
#define SYS_fstat 8
#define SYS_chdir 9
#define SYS_dup 10
#define SYS_getpid 11
#define SYS_sbrk 12
#define SYS_ms_sleep 13
#define SYS_uptime 14
#define SYS_open 15
#define SYS_write 16
#define SYS_mknod 17
#define SYS_unlink 18
#define SYS_link 19
#define SYS_mkdir 20
#define SYS_close 21
#define SYS_get_dirent 22
#define SYS_reboot 23
#define SYS_get_time 24
#define SYS_lseek 25
#define SYS_rmdir 26
#define SYS_mount 27
#define SYS_umount 28
#define SYS_ioctl 29

#define SEEK_SET 0  //< Seek from beginning of file
#define SEEK_CUR 1  //< Seek from current position
#define SEEK_END 2  //< Seek from end of file
