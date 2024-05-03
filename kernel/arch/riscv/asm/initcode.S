# SPDX-License-Identifier: MIT

# Initial process that execs /usr/bin/init.
# This code runs in user space.

#include <kernel/unistd.h>

# execv(init, argv)
.globl start
start:
    la a0, init
    la a1, argv
    li a7, SYS_execv
    ecall
    # execv only returns if there was an error

# for(;;) exit();
exit:
    # return -0xDEAD to signal the kernel that initcode returned, not /usr/bin/init
    li a0, (-0xDEAD)
    li a7, SYS_exit
    ecall
    jal exit

# char init[] = "/usr/bin/init\0";
init:
  .string "/usr/bin/init\0"

#if (__riscv_xlen == 64)

# char *argv[] = { init, 0 };
.p2align 2
argv:
    .quad init
    .quad 0

#else
// 32 bit

.string "\0"
# char *argv[] = { init, 0 };
.p2align 4
argv:
    .long init
    .long 0

#endif
