#!/usr/bin/perl -w

# SPDX-License-Identifier: MIT
# Generate usys.S, the stubs for syscalls.

print "# generated by usys.pl - do not edit\n";
print "#include <kernel/unistd.h>\n";
print ".extern errno\n";


# Call syscall
# Check if return value is < 0
#   If so, store negated return value in errno
#   Return -1 instead
sub entry {
    my $name = shift;
    if ($ARGV[0] eq "-riscv")
    {
        print ".global $name\n";
        print "${name}:\n";
        print " li a7, SYS_${name}\n";
        print " ecall\n";
        print " bge a0, zero, 1f\n";
        print " neg a0, a0\n";
        print " sw a0, errno, t0\n";
        print " li a0,-1\n";
        print "1:\n";
        print " ret\n";
    }
}

entry("fork");
entry("exit");
entry("wait");
entry("pipe");
entry("read");
entry("write");
entry("close");
entry("kill");
entry("execv");
entry("open");
entry("mknod");
entry("unlink");
entry("fstat");
entry("link");
entry("mkdir");
entry("chdir");
entry("dup");
entry("getpid");
entry("sbrk");
entry("ms_sleep");
entry("uptime");
entry("get_dirent");
entry("reboot");
entry("get_time");
entry("lseek");
entry("rmdir")
