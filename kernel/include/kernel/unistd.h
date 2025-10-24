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
#define SYS_clock_gettime 24
#define SYS_lseek 25
#define SYS_rmdir 26
#define SYS_mount 27
#define SYS_umount 28
#define SYS_ioctl 29
#define SYS_statvfs 30
#define SYS_fstatvfs 31
#define SYS_truncate 32
#define SYS_ftruncate 33
#define SYS_getresuid 34
#define SYS_getresgid 35
#define SYS_setuid 36
#define SYS_setgid 37
#define SYS_setresuid 38
#define SYS_setresgid 39
#define SYS_chown 40
#define SYS_fchown 41
#define SYS_chmod 42
#define SYS_fchmod 43
#define SYS_stat 44
#define SYS_setgroups 45
#define SYS_getgroups 46

#define SEEK_SET 0  //< Seek from beginning of file
#define SEEK_CUR 1  //< Seek from current position
#define SEEK_END 2  //< Seek from end of file
