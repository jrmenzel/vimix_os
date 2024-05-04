/* SPDX-License-Identifier: MIT */

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

// usertests has access to the internal kernel API:
#include <kernel/fs.h>
#include <mm/memlayout.h>
#include <riscv.h>

//
// Tests VIMIX system calls.  usertests without arguments runs them all
// and usertests <name> runs <name> test. The test runner creates for
// each test a process and based on the exit status of the process,
// the test runner reports "OK" or "FAILED".  Some tests result in
// kernel printing user_mode_interrupt_handler messages, which can be ignored if
// test prints "OK".
//

#define FORK_FORK_FORK_DURATION_MS 2000
#define FORK_FORK_FORK_SLEEP_MS 1000
#define SHORT_SLEEP_MS 100

#define BUFSZ ((MAX_OP_BLOCKS + 2) * BLOCK_SIZE)

char buf[BUFSZ];

#ifdef _ARCH_32BIT
// 32 bit
const size_t TEST_POINTER_ADDR_1 = 0x80000000LL;
const size_t TEST_POINTER_ADDR_2 = 0xffffffff;
#else
// 64 bit
const size_t TEST_POINTER_ADDR_1 = 0x80000000LL;
const size_t TEST_POINTER_ADDR_2 = 0xffffffffffffffff;
#endif

const char *bin_echo = "/usr/bin/echo";
const char *bin_init = "/usr/bin/init";

//
// Section with tests that run fairly quickly.  Use -q if you want to
// run just those.  With -q usertests also runs the ones that take a
// fair of time.
//

// what if you pass ridiculous pointers to system calls
// that read user memory with uvm_copy_in?
void copyin(char *s)
{
    size_t addrs[] = {TEST_POINTER_ADDR_1, TEST_POINTER_ADDR_2};

    for (size_t ai = 0; ai < 2; ai++)
    {
        void *addr = (void *)addrs[ai];

        int fd = open("copyin1", O_CREATE | O_WRONLY, 0755);
        if (fd < 0)
        {
            printf("open(copyin1) failed\n");
            exit(1);
        }
        ssize_t n = write(fd, (void *)addr, 8192);
        if (n >= 0)
        {
            printf("write(fd, %p, 8192) returned %d, not -1\n", addr, n);
            exit(1);
        }
        close(fd);
        unlink("copyin1");

        n = write(1, (char *)addr, 8192);
        if (n > 0)
        {
            printf("write(1, %p, 8192) returned %d, not -1 or 0\n", addr, n);
            exit(1);
        }

        int fds[2];
        if (pipe(fds) < 0)
        {
            printf("pipe() failed\n");
            exit(1);
        }
        n = write(fds[1], (char *)addr, 8192);
        if (n > 0)
        {
            printf("write(pipe, %p, 8192) returned %d, not -1 or 0\n", addr, n);
            exit(1);
        }
        close(fds[0]);
        close(fds[1]);
    }
}

// what if you pass ridiculous pointers to system calls
// that write user memory with uvm_copy_out?
void copyout(char *s)
{
    size_t addrs[] = {TEST_POINTER_ADDR_1, TEST_POINTER_ADDR_2};

    for (size_t ai = 0; ai < 2; ai++)
    {
        void *addr = (void *)addrs[ai];

        int fd = open("/README.md", 0);
        if (fd < 0)
        {
            printf("open(/README.md) failed\n");
            exit(1);
        }
        ssize_t n = read(fd, (void *)addr, 8192);
        if (n > 0)
        {
            printf("read(fd, %p, 8192) returned %d, not -1 or 0\n", addr, n);
            exit(1);
        }
        close(fd);

        int fds[2];
        if (pipe(fds) < 0)
        {
            printf("pipe() failed\n");
            exit(1);
        }
        n = write(fds[1], "x", 1);
        if (n != 1)
        {
            printf("pipe write failed\n");
            exit(1);
        }
        n = read(fds[0], (void *)addr, 8192);
        if (n > 0)
        {
            printf("read(pipe, %p, 8192) returned %d, not -1 or 0\n", addr, n);
            exit(1);
        }
        close(fds[0]);
        close(fds[1]);
    }
}

// what if you pass ridiculous string pointers to system calls?
void copyinstr1(char *s)
{
    size_t addrs[] = {TEST_POINTER_ADDR_1, TEST_POINTER_ADDR_2};

    for (size_t ai = 0; ai < 2; ai++)
    {
        char *addr = (char *)addrs[ai];

        int fd = open((char *)addr, O_CREATE | O_WRONLY, 0755);
        if (fd >= 0)
        {
            printf("open(%p) returned %d, not -1\n", addr, fd);
            exit(1);
        }
    }
}

// what if a string system call argument is exactly the size
// of the kernel buffer it is copied into, so that the null
// would fall just beyond the end of the kernel buffer?
void copyinstr2(char *s)
{
    char b[PATH_MAX + 1];

    for (size_t i = 0; i < PATH_MAX; i++)
    {
        b[i] = 'x';
    }
    b[PATH_MAX] = '\0';

    int ret = unlink(b);
    if (ret != -1)
    {
        printf("unlink(%s) returned %d, not -1\n", b, ret);
        exit(1);
    }

    int fd = open(b, O_CREATE | O_WRONLY, 0755);
    if (fd != -1)
    {
        printf("open(%s) returned %d, not -1\n", b, fd);
        exit(1);
    }

    ret = link(b, b);
    if (ret != -1)
    {
        printf("link(%s, %s) returned %d, not -1\n", b, b, ret);
        exit(1);
    }

    char *args[] = {"xx", 0};
    ret = execv(b, args);
    if (ret != -1)
    {
        printf("execv(%s) returned %d, not -1\n", b, fd);
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        printf("fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        long max_arg_size = sysconf(_SC_ARG_MAX);
        char *big = malloc(max_arg_size + 1);

        for (size_t i = 0; i < max_arg_size; i++)
        {
            big[i] = 'x';
        }
        big[max_arg_size] = '\0';
        char *args2[] = {big, big, big, 0};
        ret = execv(bin_echo, args2);
        free(big);
        if (ret != -1)
        {
            printf("execv(echo, BIG) returned %d, not -1\n", fd);
            exit(1);
        }
        exit(747);  // OK
    }

    int st = 0;
    wait(&st);
    st = WEXITSTATUS(st);
    if (st != 747)
    {
        printf("execv(echo, BIG) succeeded, should have failed (%d)\n", st);
        exit(1);
    }
}

// what if a string argument crosses over the end of last user page?
void copyinstr3(char *s)
{
    long page_size = sysconf(_SC_PAGE_SIZE);

    sbrk(2 * page_size);
    size_t top = (size_t)sbrk(0);
    if ((top % page_size) != 0)
    {
        sbrk(page_size - (top % page_size));
    }
    top = (size_t)sbrk(0);
    if (top % page_size)
    {
        printf("oops\n");
        exit(1);
    }

    char *b = (char *)(top - 1);
    *b = 'x';

    int ret = unlink(b);
    if (ret != -1)
    {
        printf("unlink(%s) returned %d, not -1\n", b, ret);
        exit(1);
    }

    int fd = open(b, O_CREATE | O_WRONLY, 0755);
    if (fd != -1)
    {
        printf("open(%s) returned %d, not -1\n", b, fd);
        exit(1);
    }

    ret = link(b, b);
    if (ret != -1)
    {
        printf("link(%s, %s) returned %d, not -1\n", b, b, ret);
        exit(1);
    }

    char *args[] = {"xx", 0};
    ret = execv(b, args);
    if (ret != -1)
    {
        printf("execv(%s) returned %d, not -1\n", b, fd);
        exit(1);
    }
}

// See if the kernel refuses to read/write user memory that the
// application doesn't have anymore, because it returned it.
void rwsbrk()
{
    long page_size = sysconf(_SC_PAGE_SIZE);
    size_t a = (size_t)sbrk(2 * page_size);

    if (a == TEST_POINTER_ADDR_2)
    {
        printf("sbrk(rwsbrk) failed\n");
        exit(1);
    }

    if ((size_t)sbrk(-(2 * page_size)) == TEST_POINTER_ADDR_2)
    {
        printf("sbrk(rwsbrk) shrink failed\n");
        exit(1);
    }

    FILE_DESCRIPTOR fd = open("rwsbrk", O_CREATE | O_WRONLY, 0755);
    if (fd < 0)
    {
        printf("open(rwsbrk) failed\n");
        exit(1);
    }

    ssize_t n = write(fd, (void *)(a + page_size), 1024);
    if (n >= 0)
    {
        printf("write(fd, %p, 1024) returned %d, not -1\n", a + page_size, n);
        exit(1);
    }
    close(fd);
    unlink("rwsbrk");

    fd = open("/README.md", O_RDONLY);
    if (fd < 0)
    {
        printf("open(rwsbrk) failed\n");
        exit(1);
    }
    n = read(fd, (void *)(a + page_size), 10);
    if (n >= 0)
    {
        printf("read(fd, %p, 10) returned %d, not -1\n", a + page_size, n);
        exit(1);
    }
    close(fd);

    exit(0);
}

// test O_TRUNC.
void truncate1(char *s)
{
    char buf[32];

    unlink("truncfile");
    int fd1 = open("truncfile", O_CREATE | O_WRONLY | O_TRUNC, 0755);
    write(fd1, "abcd", 4);
    close(fd1);

    int fd2 = open("truncfile", O_RDONLY);
    ssize_t n = read(fd2, buf, sizeof(buf));
    if (n != 4)
    {
        printf("%s: read %ld bytes, wanted 4\n", s, n);
        exit(1);
    }

    fd1 = open("truncfile", O_WRONLY | O_TRUNC);

    int fd3 = open("truncfile", O_RDONLY);
    n = read(fd3, buf, sizeof(buf));
    if (n != 0)
    {
        printf("aaa fd3=%d\n", fd3);
        printf("%s: read %ld bytes, wanted 0\n", s, n);
        exit(1);
    }

    n = read(fd2, buf, sizeof(buf));
    if (n != 0)
    {
        printf("bbb fd2=%d\n", fd2);
        printf("%s: read %ld bytes, wanted 0\n", s, n);
        exit(1);
    }

    write(fd1, "abcdef", 6);

    n = read(fd3, buf, sizeof(buf));
    if (n != 6)
    {
        printf("%s: read %ld bytes, wanted 6\n", s, n);
        exit(1);
    }

    n = read(fd2, buf, sizeof(buf));
    if (n != 2)
    {
        printf("%s: read %ld bytes, wanted 2\n", s, n);
        exit(1);
    }

    unlink("truncfile");

    close(fd1);
    close(fd2);
    close(fd3);
}

// write to an open FD whose file has just been truncated.
// this causes a write at an offset beyond the end of the file.
// such writes fail on VIMIX (unlike POSIX) but at least
// they don't crash.
void truncate2(char *s)
{
    unlink("truncfile");

    int fd1 = open("truncfile", O_CREATE | O_TRUNC | O_WRONLY, 0755);
    write(fd1, "abcd", 4);

    int fd2 = open("truncfile", O_TRUNC | O_WRONLY);

    ssize_t n = write(fd1, "x", 1);
    if (n != -1)
    {
        printf("%s: write returned %ld, expected -1\n", s, n);
        exit(1);
    }

    unlink("truncfile");
    close(fd1);
    close(fd2);
}

void truncate3(char *s)
{
    close(open("truncfile", O_CREATE | O_TRUNC | O_WRONLY, 0755));

    pid_t pid = fork();
    if (pid < 0)
    {
        printf("%s: fork failed\n", s);
        exit(1);
    }

    if (pid == 0)
    {
        for (size_t i = 0; i < 100; i++)
        {
            char buf[32];
            int fd = open("truncfile", O_WRONLY);
            if (fd < 0)
            {
                printf("%s: open failed\n", s);
                exit(1);
            }
            ssize_t n = write(fd, "1234567890", 10);
            if (n != 10)
            {
                printf("%s: write got %ld, expected 10\n", s, n);
                exit(1);
            }
            close(fd);
            fd = open("truncfile", O_RDONLY);
            read(fd, buf, sizeof(buf));
            close(fd);
        }
        exit(0);
    }

    for (size_t i = 0; i < 150; i++)
    {
        int fd = open("truncfile", O_CREATE | O_WRONLY | O_TRUNC, 0755);
        if (fd < 0)
        {
            printf("%s: open failed\n", s);
            exit(1);
        }
        ssize_t n = write(fd, "xxx", 3);
        if (n != 3)
        {
            printf("%s: write got %ld, expected 3\n", s, n);
            exit(1);
        }
        close(fd);
    }

    int32_t xstatus;
    wait(&xstatus);
    xstatus = WEXITSTATUS(xstatus);
    unlink("truncfile");
    exit(xstatus);
}

// does chdir() call inode_put(p->cwd) in a transaction?
void iputtest(char *s)
{
    if (mkdir("iputdir", 0755) < 0)
    {
        printf("%s: mkdir failed\n", s);
        exit(1);
    }
    if (chdir("iputdir") < 0)
    {
        printf("%s: chdir iputdir failed\n", s);
        exit(1);
    }
    if (unlink("../iputdir") < 0)
    {
        printf("%s: unlink ../iputdir failed\n", s);
        exit(1);
    }
    if (chdir("/utests-tmp") < 0)
    {
        printf("%s: chdir / failed\n", s);
        exit(1);
    }
}

// does exit() call inode_put(p->cwd) in a transaction?
void exitiputtest(char *s)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("%s: fork failed\n", s);
        exit(1);
    }
    if (pid == 0)
    {
        if (mkdir("iputdir", 0755) < 0)
        {
            printf("%s: mkdir failed\n", s);
            exit(1);
        }
        if (chdir("iputdir") < 0)
        {
            printf("%s: child chdir failed\n", s);
            exit(1);
        }
        if (unlink("../iputdir") < 0)
        {
            printf("%s: unlink ../iputdir failed\n", s);
            exit(1);
        }
        exit(0);
    }

    int32_t xstatus;
    wait(&xstatus);
    xstatus = WEXITSTATUS(xstatus);
    exit(xstatus);
}

// does the error path in open() for attempt to write a
// directory call inode_put() in a transaction?
// needs a hacked kernel that pauses just after the inode_from_path()
// call in sys_open():
//    if((ip = inode_from_path(path)) == 0)
//      return -1;
//    {
//      int i;
//      for(i = 0; i < 10000; i++)
//        yield();
//    }
void openiputtest(char *s)
{
    if (mkdir("oidir", 0755) < 0)
    {
        printf("%s: mkdir oidir failed\n", s);
        exit(1);
    }
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("%s: fork failed\n", s);
        exit(1);
    }
    if (pid == 0)
    {
        int fd = open("oidir", O_RDWR);
        if (fd >= 0)
        {
            printf("%s: open directory for write succeeded\n", s);
            exit(1);
        }
        exit(0);
    }
    usleep(SHORT_SLEEP_MS * 1000);
    if (unlink("oidir") != 0)
    {
        printf("%s: unlink failed\n", s);
        exit(1);
    }

    int32_t xstatus;
    wait(&xstatus);
    xstatus = WEXITSTATUS(xstatus);
    exit(xstatus);
}

// simple file system tests

void opentest(char *s)
{
    int fd = open("/usr/bin/echo", 0);
    if (fd < 0)
    {
        printf("%s: open /usr/bin/echo failed!\n", s);
        exit(1);
    }
    close(fd);
    fd = open("doesnotexist", 0);
    if (fd >= 0)
    {
        printf("%s: open doesnotexist succeeded!\n", s);
        exit(1);
    }
}

void writetest(char *s)
{
    const size_t N = 100;
    const size_t SZ = 10;

    int fd = open("small", O_CREATE | O_RDWR, 0755);
    if (fd < 0)
    {
        printf("%s: error: creat small failed!\n", s);
        exit(1);
    }
    for (size_t i = 0; i < N; i++)
    {
        if (write(fd, "aaaaaaaaaa", SZ) != SZ)
        {
            printf("%s: error: write aa %ld new file failed\n", s, i);
            exit(1);
        }
        if (write(fd, "bbbbbbbbbb", SZ) != SZ)
        {
            printf("%s: error: write bb %ld new file failed\n", s, i);
            exit(1);
        }
    }
    close(fd);
    fd = open("small", O_RDONLY);
    if (fd < 0)
    {
        printf("%s: error: open small failed!\n", s);
        exit(1);
    }
    ssize_t i = read(fd, buf, N * SZ * 2);
    if (i != N * SZ * 2)
    {
        printf("%s: read failed\n", s);
        exit(1);
    }
    close(fd);

    if (unlink("small") < 0)
    {
        printf("%s: unlink small failed\n", s);
        exit(1);
    }
}

void writebig(char *s)
{
    int fd = open("big", O_CREATE | O_RDWR, 0755);
    if (fd < 0)
    {
        printf("%s: error: creat big failed!\n", s);
        exit(1);
    }

    for (size_t i = 0; i < MAXFILE; i++)
    {
        ((int *)buf)[0] = i;
        if (write(fd, buf, BLOCK_SIZE) != BLOCK_SIZE)
        {
            printf("%s: error: write big file failed\n", s, i);
            exit(1);
        }
    }

    close(fd);

    fd = open("big", O_RDONLY);
    if (fd < 0)
    {
        printf("%s: error: open big failed!\n", s);
        exit(1);
    }

    int32_t n = 0;
    while (true)
    {
        ssize_t i = read(fd, buf, BLOCK_SIZE);
        if (i == 0)
        {
            if (n == MAXFILE - 1)
            {
                printf("%s: read only %d blocks from big", s, n);
                exit(1);
            }
            break;
        }
        else if (i != BLOCK_SIZE)
        {
            printf("%s: read failed %d\n", s, i);
            exit(1);
        }
        if (((int *)buf)[0] != n)
        {
            printf("%s: read content of block %d is %d\n", s, n,
                   ((int *)buf)[0]);
            exit(1);
        }
        n++;
    }
    close(fd);
    if (unlink("big") < 0)
    {
        printf("%s: unlink big failed\n", s);
        exit(1);
    }
}

// many creates, followed by unlink test
void createtest(char *s)
{
    const size_t N = 52;

    char name[3];
    name[0] = 'a';
    name[2] = '\0';
    for (size_t i = 0; i < N; i++)
    {
        name[1] = '0' + i;
        int fd = open(name, O_CREATE | O_RDWR, 0755);
        close(fd);
    }
    name[0] = 'a';
    name[2] = '\0';
    for (size_t i = 0; i < N; i++)
    {
        name[1] = '0' + i;
        unlink(name);
    }
}

void dirtest(char *s)
{
    if (mkdir("dir0", 0755) < 0)
    {
        printf("%s: mkdir failed\n", s);
        exit(1);
    }

    if (chdir("dir0") < 0)
    {
        printf("%s: chdir dir0 failed\n", s);
        exit(1);
    }

    if (chdir("..") < 0)
    {
        printf("%s: chdir .. failed\n", s);
        exit(1);
    }

    if (unlink("dir0") < 0)
    {
        printf("%s: unlink dir0 failed\n", s);
        exit(1);
    }
}

void exectest(char *s)
{
    char *echoargv[] = {"/usr/bin/echo", "OK", 0};

    unlink("echo-ok");
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("%s: fork failed\n", s);
        exit(1);
    }
    if (pid == 0)
    {
        close(1);
        int fd = open("echo-ok", O_CREATE | O_WRONLY, 0755);
        if (fd < 0)
        {
            printf("%s: create failed\n", s);
            exit(1);
        }
        if (fd != 1)
        {
            printf("%s: wrong fd\n", s);
            exit(1);
        }
        if (execv(bin_echo, echoargv) < 0)
        {
            printf("%s: execv echo failed\n", s);
            exit(1);
        }
        // won't get to here
    }

    int32_t xstatus;
    if (wait(&xstatus) != pid)
    {
        printf("%s: wait failed!\n", s);
    }
    xstatus = WEXITSTATUS(xstatus);
    if (xstatus != 0)
    {
        exit(xstatus);
    }

    int fd = open("echo-ok", O_RDONLY);
    char buf[3];
    if (fd < 0)
    {
        printf("%s: open failed\n", s);
        exit(1);
    }
    if (read(fd, buf, 2) != 2)
    {
        printf("%s: read failed\n", s);
        exit(1);
    }
    unlink("echo-ok");

    if (buf[0] == 'O' && buf[1] == 'K')
    {
        exit(0);
    }
    else
    {
        printf("%s: wrong output\n", s);
        exit(1);
    }
}

// simple fork and pipe read/write

void pipe1(char *s)
{
    const size_t N = 5;
    const size_t SZ = 1033;

    int fds[2];
    if (pipe(fds) != 0)
    {
        printf("%s: pipe() failed\n", s);
        exit(1);
    }

    pid_t pid = fork();
    int seq = 0;
    if (pid == 0)
    {
        close(fds[0]);
        for (size_t n = 0; n < N; n++)
        {
            for (size_t i = 0; i < SZ; i++)
            {
                buf[i] = seq++;
            }
            if (write(fds[1], buf, SZ) != SZ)
            {
                printf("%s: pipe1 oops 1\n", s);
                exit(1);
            }
        }
        exit(0);
    }
    else if (pid > 0)
    {
        close(fds[1]);
        size_t total = 0;
        size_t cc = 1;
        ssize_t n = read(fds[0], buf, cc);
        while (n > 0)
        {
            for (size_t i = 0; i < n; i++)
            {
                if ((buf[i] & 0xff) != (seq++ & 0xff))
                {
                    printf("%s: pipe1 oops 2\n", s);
                    return;
                }
            }
            total += n;
            cc = cc * 2;
            if (cc > sizeof(buf))
            {
                cc = sizeof(buf);
            }

            n = read(fds[0], buf, cc);
        }

        if (total != N * SZ)
        {
            printf("%s: pipe1 oops 3 total %d\n", total);
            exit(1);
        }
        close(fds[0]);

        int32_t xstatus;
        wait(&xstatus);
        xstatus = WEXITSTATUS(xstatus);
        exit(xstatus);
    }
    else
    {
        printf("%s: fork() failed\n", s);
        exit(1);
    }
}

// test if child is killed (status = -1)
// assumes to run out of processes
void killstatus(char *s)
{
    for (size_t i = 0; i < 25; i++)
    {
        pid_t pid1 = fork();
        if (pid1 < 0)
        {
            printf("%s: fork failed\n", s);
            exit(1);
        }
        if (pid1 == 0)
        {
            while (true)
            {
                getpid();
            }
            exit(0);
        }
        usleep(SHORT_SLEEP_MS * 1000);
        kill(pid1, SIGKILL);

        int32_t xstatus;
        wait(&xstatus);
        xstatus = WEXITSTATUS(xstatus);
        if (xstatus != -1)
        {
            printf("%s: status should be -1\n", s);
            exit(1);
        }
    }
    exit(0);
}

// meant to be run w/ at most two CPUs
void preempt(char *s)
{
    pid_t pid1 = fork();
    if (pid1 < 0)
    {
        printf("%s: fork failed", s);
        exit(1);
    }
    if (pid1 == 0)
    {
        while (true)
        {
        }
    }

    pid_t pid2 = fork();
    if (pid2 < 0)
    {
        printf("%s: fork failed\n", s);
        exit(1);
    }
    if (pid2 == 0)
    {
        while (true)
        {
        }
    }

    int pfds[2];
    pipe(pfds);

    pid_t pid3 = fork();
    if (pid3 < 0)
    {
        printf("%s: fork failed\n", s);
        exit(1);
    }
    if (pid3 == 0)
    {
        close(pfds[0]);
        if (write(pfds[1], "x", 1) != 1)
        {
            printf("%s: preempt write error", s);
        }
        close(pfds[1]);
        while (true)
        {
        }
    }

    close(pfds[1]);
    if (read(pfds[0], buf, sizeof(buf)) != 1)
    {
        printf("%s: preempt read error", s);
        return;
    }
    close(pfds[0]);

    printf("kill... ");
    kill(pid1, SIGKILL);
    kill(pid2, SIGKILL);
    kill(pid3, SIGKILL);

    printf("wait... ");
    wait(NULL);
    wait(NULL);
    wait(NULL);
}

// try to find any races between exit and wait
void exitwait(char *s)
{
    for (size_t i = 0; i < 100; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            printf("%s: fork failed\n", s);
            exit(1);
        }
        if (pid)
        {
            int32_t xstate;
            if (wait(&xstate) != pid)
            {
                printf("%s: wait wrong pid\n", s);
                exit(1);
            }
            xstate = WEXITSTATUS(xstate);
            if (i != xstate)
            {
                printf("%s: wait wrong exit status\n", s);
                exit(1);
            }
        }
        else
        {
            exit(i);
        }
    }
}

// try to find races in the reparenting
// code that handles a parent exiting
// when it still has live children.
void reparent(char *s)
{
    pid_t master_pid = getpid();
    for (size_t i = 0; i < 200; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            printf("%s: fork failed\n", s);
            exit(1);
        }
        if (pid)
        {
            if (wait(NULL) != pid)
            {
                printf("%s: wait wrong pid\n", s);
                exit(1);
            }
        }
        else
        {
            pid_t pid2 = fork();
            if (pid2 < 0)
            {
                kill(master_pid, SIGKILL);
                exit(1);
            }
            exit(0);
        }
    }
    exit(0);
}

// what if two children exit() at the same time?
void twochildren(char *s)
{
    for (size_t i = 0; i < 1000; i++)
    {
        pid_t pid1 = fork();
        if (pid1 < 0)
        {
            printf("%s: fork failed\n", s);
            exit(1);
        }
        if (pid1 == 0)
        {
            exit(0);
        }
        else
        {
            pid_t pid2 = fork();
            if (pid2 < 0)
            {
                printf("%s: fork failed\n", s);
                exit(1);
            }
            if (pid2 == 0)
            {
                exit(0);
            }
            else
            {
                wait(NULL);
                wait(NULL);
            }
        }
    }
}

// concurrent forks to try to expose locking bugs.
void forkfork(char *s)
{
    const size_t N = 2;

    for (size_t i = 0; i < N; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            printf("%s: fork failed", s);
            exit(1);
        }
        if (pid == 0)
        {
            for (size_t j = 0; j < 200; j++)
            {
                pid_t pid1 = fork();
                if (pid1 < 0)
                {
                    exit(1);
                }
                if (pid1 == 0)
                {
                    exit(0);
                }
                wait(NULL);
            }
            exit(0);
        }
    }

    for (size_t i = 0; i < N; i++)
    {
        int32_t xstatus;
        wait(&xstatus);
        xstatus = WEXITSTATUS(xstatus);
        if (xstatus != 0)
        {
            printf("%s: fork in child failed", s);
            exit(1);
        }
    }
}

void forkforkfork(char *s)
{
    unlink("stopforking");

    pid_t pid = fork();
    if (pid < 0)
    {
        printf("%s: fork failed", s);
        exit(1);
    }
    if (pid == 0)
    {
        while (true)
        {
            int fd = open("stopforking", 0);
            if (fd >= 0)
            {
                exit(0);
            }
            if (fork() < 0)
            {
                close(open("stopforking", O_CREATE | O_RDWR, 0755));
            }
        }

        exit(0);
    }

    usleep(FORK_FORK_FORK_DURATION_MS * 1000);
    close(open("stopforking", O_CREATE | O_RDWR, 0755));
    wait(NULL);
    usleep(FORK_FORK_FORK_SLEEP_MS * 1000);
}

// regression test. does reparent() violate the parent-then-child
// locking order when giving away a child to init, so that exit()
// deadlocks against init's wait()? also used to trigger a "panic:
// release" due to exit() releasing a different p->parent->lock than
// it acquired.
void reparent2(char *s)
{
    for (size_t i = 0; i < 800; i++)
    {
        pid_t pid1 = fork();
        if (pid1 < 0)
        {
            printf("fork failed\n");
            exit(1);
        }
        if (pid1 == 0)
        {
            fork();
            fork();
            exit(0);
        }
        wait(NULL);
    }

    exit(0);
}

// allocate all mem, free it, and allocate again
void mem(char *s)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        void *m1 = NULL;
        void *m2 = NULL;
        while ((m2 = malloc(10001)) != NULL)
        {
            *(char **)m2 = m1;
            m1 = m2;
        }
        while (m1)
        {
            m2 = *(char **)m1;
            free(m1);
            m1 = m2;
        }
        m1 = malloc(1024 * 20);
        if (m1 == NULL)
        {
            printf("%s: couldn't allocate mem?!!\n", s);
            exit(1);
        }
        free(m1);
        exit(0);
    }
    else
    {
        int32_t xstatus;
        wait(&xstatus);
        xstatus = WEXITSTATUS(xstatus);
        if (xstatus == -1)
        {
            // probably page fault, so might be lazy lab,
            // so OK.
            exit(0);
        }
        exit(xstatus);
    }
}

// More file system tests

// two processes write to the same file descriptor
// is the offset shared? does inode locking work?
void sharedfd(char *s)
{
    const size_t N = 100;
    const size_t SZ = 10;
    char buf[SZ];

    unlink("sharedfd");
    int fd = open("sharedfd", O_CREATE | O_RDWR, 0755);
    if (fd < 0)
    {
        printf("%s: cannot open sharedfd for writing", s);
        exit(1);
    }
    pid_t pid = fork();
    memset(buf, pid == 0 ? 'c' : 'p', sizeof(buf));
    for (size_t i = 0; i < N; i++)
    {
        if (write(fd, buf, sizeof(buf)) != sizeof(buf))
        {
            printf("%s: write sharedfd failed\n", s);
            exit(1);
        }
    }
    if (pid == 0)
    {
        exit(0);
    }
    else
    {
        int32_t xstatus;
        wait(&xstatus);
        xstatus = WEXITSTATUS(xstatus);
        if (xstatus != 0)
        {
            exit(xstatus);
        }
    }

    close(fd);
    fd = open("sharedfd", 0);
    if (fd < 0)
    {
        printf("%s: cannot open sharedfd for reading\n", s);
        exit(1);
    }

    size_t nc = 0;
    size_t np = 0;
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        for (size_t i = 0; i < sizeof(buf); i++)
        {
            if (buf[i] == 'c') nc++;
            if (buf[i] == 'p') np++;
        }
    }
    close(fd);
    unlink("sharedfd");
    if (nc == N * SZ && np == N * SZ)
    {
        exit(0);
    }
    else
    {
        printf("%s: nc/np test fails\n", s);
        exit(1);
    }
}

// four processes write different files at the same
// time, to test block allocation.
void fourfiles(char *s)
{
    char *names[] = {"f0", "f1", "f2", "f3"};

    const size_t N = 12;
    const size_t NCHILD = 4;
    const size_t SZ = 500;

    for (size_t pi = 0; pi < NCHILD; pi++)
    {
        char *fname = names[pi];
        unlink(fname);

        pid_t pid = fork();
        if (pid < 0)
        {
            printf("fork failed\n", s);
            exit(1);
        }

        if (pid == 0)
        {
            int fd = open(fname, O_CREATE | O_RDWR, 0755);
            if (fd < 0)
            {
                printf("%s: create failed\n", s);
                exit(1);
            }

            memset(buf, '0' + pi, SZ);
            for (size_t i = 0; i < N; i++)
            {
                ssize_t n = write(fd, buf, SZ);
                if (n != SZ)
                {
                    printf("write failed %d\n", n);
                    exit(1);
                }
            }
            exit(0);
        }
    }

    int32_t xstatus;
    for (size_t pi = 0; pi < NCHILD; pi++)
    {
        wait(&xstatus);
        xstatus = WEXITSTATUS(xstatus);
        if (xstatus != 0) exit(xstatus);
    }

    for (size_t i = 0; i < NCHILD; i++)
    {
        char *fname = names[i];
        int fd = open(fname, 0);
        int32_t total = 0;
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0)
        {
            for (size_t j = 0; j < n; j++)
            {
                if (buf[j] != '0' + i)
                {
                    printf("wrong char\n", s);
                    exit(1);
                }
            }
            total += n;
        }
        close(fd);
        if (total != N * SZ)
        {
            printf("wrong length %d\n", total);
            exit(1);
        }
        unlink(fname);
    }
}

// four processes create and delete different files in same directory
void createdelete(char *s)
{
    const size_t N = 20;
    const size_t NCHILD = 4;

    char name[32];

    for (size_t pi = 0; pi < NCHILD; pi++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            printf("%s: fork failed\n", s);
            exit(1);
        }

        if (pid == 0)
        {
            name[0] = 'p' + pi;
            name[2] = '\0';
            for (size_t i = 0; i < N; i++)
            {
                name[1] = '0' + i;
                int fd = open(name, O_CREATE | O_RDWR, 0755);
                if (fd < 0)
                {
                    printf("%s: create failed\n", s);
                    exit(1);
                }
                close(fd);
                if (i > 0 && (i % 2) == 0)
                {
                    name[1] = '0' + (i / 2);
                    if (unlink(name) < 0)
                    {
                        printf("%s: unlink failed\n", s);
                        exit(1);
                    }
                }
            }
            exit(0);
        }
    }

    int32_t xstatus;
    for (size_t pi = 0; pi < NCHILD; pi++)
    {
        wait(&xstatus);
        xstatus = WEXITSTATUS(xstatus);
        if (xstatus != 0)
        {
            exit(1);
        }
    }

    name[0] = name[1] = name[2] = 0;
    for (size_t i = 0; i < N; i++)
    {
        for (size_t pi = 0; pi < NCHILD; pi++)
        {
            name[0] = 'p' + pi;
            name[1] = '0' + i;
            int fd = open(name, 0);
            if ((i == 0 || i >= N / 2) && fd < 0)
            {
                printf("%s: oops createdelete %s didn't exist\n", s, name);
                exit(1);
            }
            else if ((i >= 1 && i < N / 2) && fd >= 0)
            {
                printf("%s: oops createdelete %s did exist\n", s, name);
                exit(1);
            }
            if (fd >= 0)
            {
                close(fd);
            }
        }
    }

    for (size_t i = 0; i < N; i++)
    {
        for (size_t pi = 0; pi < NCHILD; pi++)
        {
            name[0] = 'p' + i;
            name[1] = '0' + i;
            unlink(name);
        }
    }
}

// can I unlink a file and still read it?
void unlinkread(char *s)
{
    const size_t SZ = 5;

    int fd = open("unlinkread", O_CREATE | O_RDWR, 0755);
    if (fd < 0)
    {
        printf("%s: create unlinkread failed\n", s);
        exit(1);
    }
    write(fd, "hello", SZ);
    close(fd);

    fd = open("unlinkread", O_RDWR);
    if (fd < 0)
    {
        printf("%s: open unlinkread failed\n", s);
        exit(1);
    }
    if (unlink("unlinkread") != 0)
    {
        printf("%s: unlink unlinkread failed\n", s);
        exit(1);
    }

    int fd1 = open("unlinkread", O_CREATE | O_RDWR, 0755);
    write(fd1, "yyy", 3);
    close(fd1);

    if (read(fd, buf, sizeof(buf)) != SZ)
    {
        printf("%s: unlinkread read failed", s);
        exit(1);
    }
    if (buf[0] != 'h')
    {
        printf("%s: unlinkread wrong data\n", s);
        exit(1);
    }
    if (write(fd, buf, 10) != 10)
    {
        printf("%s: unlinkread write failed\n", s);
        exit(1);
    }
    close(fd);
    unlink("unlinkread");
}

void linktest(char *s)
{
    const size_t SZ = 5;

    unlink("lf1");
    unlink("lf2");

    int fd = open("lf1", O_CREATE | O_RDWR, 0755);
    if (fd < 0)
    {
        printf("%s: create lf1 failed\n", s);
        exit(1);
    }
    if (write(fd, "hello", SZ) != SZ)
    {
        printf("%s: write lf1 failed\n", s);
        exit(1);
    }
    close(fd);

    if (link("lf1", "lf2") < 0)
    {
        printf("%s: link lf1 lf2 failed\n", s);
        exit(1);
    }
    unlink("lf1");

    if (open("lf1", 0) >= 0)
    {
        printf("%s: unlinked lf1 but it is still there!\n", s);
        exit(1);
    }

    fd = open("lf2", 0);
    if (fd < 0)
    {
        printf("%s: open lf2 failed\n", s);
        exit(1);
    }
    if (read(fd, buf, sizeof(buf)) != SZ)
    {
        printf("%s: read lf2 failed\n", s);
        exit(1);
    }
    close(fd);

    if (link("lf2", "lf2") >= 0)
    {
        printf("%s: link lf2 lf2 succeeded! oops\n", s);
        exit(1);
    }

    unlink("lf2");
    if (link("lf2", "lf1") >= 0)
    {
        printf("%s: link non-existent succeeded! oops\n", s);
        exit(1);
    }

    if (link(".", "lf1") >= 0)
    {
        printf("%s: link . lf1 succeeded! oops\n", s);
        exit(1);
    }
}

// test concurrent create/link/unlink of the same file
void concreate(char *s)
{
    const size_t N = 40;

    char fa[N];
    struct
    {
        uint16_t inum;
        char name[XV6_NAME_MAX];
    } de;

    char file[3];
    file[0] = 'C';
    file[2] = '\0';

    for (size_t i = 0; i < N; i++)
    {
        file[1] = '0' + i;
        unlink(file);
        pid_t pid = fork();
        if (pid && (i % 3) == 1)
        {
            link("C0", file);
        }
        else if (pid == 0 && (i % 5) == 1)
        {
            link("C0", file);
        }
        else
        {
            int fd = open(file, O_CREATE | O_RDWR, 0755);
            if (fd < 0)
            {
                printf("concreate create %s failed\n", file);
                exit(1);
            }
            close(fd);
        }
        if (pid == 0)
        {
            exit(0);
        }
        else
        {
            int32_t xstatus;
            wait(&xstatus);
            xstatus = WEXITSTATUS(xstatus);
            if (xstatus != 0) exit(1);
        }
    }

    memset(fa, 0, sizeof(fa));
    int fd = open(".", 0);

    size_t n = 0;
    while (read(fd, &de, sizeof(de)) > 0)
    {
        if (de.inum == 0)
        {
            continue;
        }

        if (de.name[0] == 'C' && de.name[2] == '\0')
        {
            size_t i = de.name[1] - '0';
            if (i < 0 || i >= sizeof(fa))
            {
                printf("%s: concreate weird file %s\n", s, de.name);
                exit(1);
            }
            if (fa[i])
            {
                printf("%s: concreate duplicate file %s\n", s, de.name);
                exit(1);
            }
            fa[i] = 1;
            n++;
        }
    }
    close(fd);

    if (n != N)
    {
        printf("%s: concreate not enough files in directory listing\n", s);
        exit(1);
    }

    for (size_t i = 0; i < N; i++)
    {
        file[1] = '0' + i;
        pid_t pid = fork();
        if (pid < 0)
        {
            printf("%s: fork failed\n", s);
            exit(1);
        }
        if (((i % 3) == 0 && pid == 0) || ((i % 3) == 1 && pid != 0))
        {
            close(open(file, 0));
            close(open(file, 0));
            close(open(file, 0));
            close(open(file, 0));
            close(open(file, 0));
            close(open(file, 0));
        }
        else
        {
            unlink(file);
            unlink(file);
            unlink(file);
            unlink(file);
            unlink(file);
            unlink(file);
        }
        if (pid == 0)
            exit(0);
        else
            wait(NULL);
    }
}

// another concurrent link/unlink/create test,
// to look for deadlocks.
void linkunlink(char *s)
{
    unlink("x");
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("%s: fork failed\n", s);
        exit(1);
    }

    uint32_t x = (pid ? 1 : 97);
    for (size_t i = 0; i < 100; i++)
    {
        x = x * 1103515245 + 12345;
        if ((x % 3) == 0)
        {
            close(open("x", O_RDWR | O_CREATE, 0755));
        }
        else if ((x % 3) == 1)
        {
            link("cat", "x");
        }
        else
        {
            unlink("x");
        }
    }

    if (pid)
        wait(NULL);
    else
        exit(0);
}

void subdir(char *s)
{
    int fd, cc;

    unlink("ff");
    if (mkdir("dd", 0755) != 0)
    {
        printf("%s: mkdir dd failed\n", s);
        exit(1);
    }

    fd = open("dd/ff", O_CREATE | O_RDWR, 0755);
    if (fd < 0)
    {
        printf("%s: create dd/ff failed\n", s);
        exit(1);
    }
    write(fd, "ff", 2);
    close(fd);

    if (unlink("dd") >= 0)
    {
        printf("%s: unlink dd (non-empty dir) succeeded!\n", s);
        exit(1);
    }

    if (mkdir("/utests-tmp/dd/dd", 0755) != 0)
    {
        printf("%s: subdir mkdir /utests-tmp/dd/dd failed\n", s);
        exit(1);
    }

    fd = open("dd/dd/ff", O_CREATE | O_RDWR, 0755);
    if (fd < 0)
    {
        printf("%s: create dd/dd/ff failed\n", s);
        exit(1);
    }
    write(fd, "FF", 2);
    close(fd);

    fd = open("dd/dd/../ff", 0);
    if (fd < 0)
    {
        printf("%s: open dd/dd/../ff failed\n", s);
        exit(1);
    }
    cc = read(fd, buf, sizeof(buf));
    if (cc != 2 || buf[0] != 'f')
    {
        printf("%s: dd/dd/../ff wrong content\n", s);
        exit(1);
    }
    close(fd);

    if (link("dd/dd/ff", "dd/dd/ffff") != 0)
    {
        printf("link dd/dd/ff dd/dd/ffff failed\n", s);
        exit(1);
    }

    if (unlink("dd/dd/ff") != 0)
    {
        printf("%s: unlink dd/dd/ff failed\n", s);
        exit(1);
    }
    if (open("dd/dd/ff", O_RDONLY) >= 0)
    {
        printf("%s: open (unlinked) dd/dd/ff succeeded\n", s);
        exit(1);
    }

    if (chdir("dd") != 0)
    {
        printf("%s: chdir dd failed\n", s);
        exit(1);
    }
    if (chdir("dd/../../dd") != 0)
    {
        printf("%s: chdir dd/../../dd failed\n", s);
        exit(1);
    }
    if (chdir("dd/../../../utests-tmp/dd") != 0)
    {
        printf("%s: chdir dd/../../../utests-tmp/dd failed\n", s);
        exit(1);
    }
    if (chdir("./..") != 0)
    {
        printf("%s: chdir ./.. failed\n", s);
        exit(1);
    }

    fd = open("dd/dd/ffff", 0);
    if (fd < 0)
    {
        printf("%s: open dd/dd/ffff failed\n", s);
        exit(1);
    }
    if (read(fd, buf, sizeof(buf)) != 2)
    {
        printf("%s: read dd/dd/ffff wrong len\n", s);
        exit(1);
    }
    close(fd);

    if (open("dd/dd/ff", O_RDONLY) >= 0)
    {
        printf("%s: open (unlinked) dd/dd/ff succeeded!\n", s);
        exit(1);
    }

    if (open("dd/ff/ff", O_CREATE | O_RDWR, 0755) >= 0)
    {
        printf("%s: create dd/ff/ff succeeded!\n", s);
        exit(1);
    }
    if (open("dd/xx/ff", O_CREATE | O_RDWR, 0755) >= 0)
    {
        printf("%s: create dd/xx/ff succeeded!\n", s);
        exit(1);
    }
    if (open("dd", O_CREATE, 0755) >= 0)
    {
        printf("%s: create dd succeeded!\n", s);
        exit(1);
    }
    if (open("dd", O_RDWR) >= 0)
    {
        printf("%s: open dd rdwr succeeded!\n", s);
        exit(1);
    }
    if (open("dd", O_WRONLY) >= 0)
    {
        printf("%s: open dd wronly succeeded!\n", s);
        exit(1);
    }
    if (link("dd/ff/ff", "dd/dd/xx") == 0)
    {
        printf("%s: link dd/ff/ff dd/dd/xx succeeded!\n", s);
        exit(1);
    }
    if (link("dd/xx/ff", "dd/dd/xx") == 0)
    {
        printf("%s: link dd/xx/ff dd/dd/xx succeeded!\n", s);
        exit(1);
    }
    if (link("dd/ff", "dd/dd/ffff") == 0)
    {
        printf("%s: link dd/ff dd/dd/ffff succeeded!\n", s);
        exit(1);
    }
    if (mkdir("dd/ff/ff", 0755) == 0)
    {
        printf("%s: mkdir dd/ff/ff succeeded!\n", s);
        exit(1);
    }
    if (mkdir("dd/xx/ff", 0755) == 0)
    {
        printf("%s: mkdir dd/xx/ff succeeded!\n", s);
        exit(1);
    }
    if (mkdir("dd/dd/ffff", 0755) == 0)
    {
        printf("%s: mkdir dd/dd/ffff succeeded!\n", s);
        exit(1);
    }
    if (unlink("dd/xx/ff") == 0)
    {
        printf("%s: unlink dd/xx/ff succeeded!\n", s);
        exit(1);
    }
    if (unlink("dd/ff/ff") == 0)
    {
        printf("%s: unlink dd/ff/ff succeeded!\n", s);
        exit(1);
    }
    if (chdir("dd/ff") == 0)
    {
        printf("%s: chdir dd/ff succeeded!\n", s);
        exit(1);
    }
    if (chdir("dd/xx") == 0)
    {
        printf("%s: chdir dd/xx succeeded!\n", s);
        exit(1);
    }

    if (unlink("dd/dd/ffff") != 0)
    {
        printf("%s: unlink dd/dd/ffff failed\n", s);
        exit(1);
    }
    if (unlink("dd/ff") != 0)
    {
        printf("%s: unlink dd/ff failed\n", s);
        exit(1);
    }
    if (unlink("dd") == 0)
    {
        printf("%s: unlink non-empty dd succeeded!\n", s);
        exit(1);
    }
    if (unlink("dd/dd") < 0)
    {
        printf("%s: unlink dd/dd failed\n", s);
        exit(1);
    }
    if (unlink("dd") < 0)
    {
        printf("%s: unlink dd failed\n", s);
        exit(1);
    }
}

// test writes that are larger than the log.
void bigwrite(char *s)
{
    unlink("bigwrite");
    for (size_t sz = 499; sz < (MAX_OP_BLOCKS + 2) * BLOCK_SIZE; sz += 471)
    {
        int fd = open("bigwrite", O_CREATE | O_RDWR, 0755);
        if (fd < 0)
        {
            printf("%s: cannot create bigwrite\n", s);
            exit(1);
        }

        for (size_t i = 0; i < 2; i++)
        {
            ssize_t cc = write(fd, buf, sz);
            if (cc != sz)
            {
                printf("%s: write(%ld) ret %ld\n", s, sz, cc);
                exit(1);
            }
        }
        close(fd);
        unlink("bigwrite");
    }
}

void bigfile(char *s)
{
    const size_t N = 20;
    const size_t SZ = 600;

    unlink("bigfile.dat");
    int fd = open("bigfile.dat", O_CREATE | O_RDWR, 0755);
    if (fd < 0)
    {
        printf("%s: cannot create bigfile", s);
        exit(1);
    }
    for (size_t i = 0; i < N; i++)
    {
        memset(buf, i, SZ);
        if (write(fd, buf, SZ) != SZ)
        {
            printf("%s: write bigfile failed\n", s);
            exit(1);
        }
    }
    close(fd);

    fd = open("bigfile.dat", 0);
    if (fd < 0)
    {
        printf("%s: cannot open bigfile\n", s);
        exit(1);
    }

    size_t total = 0;
    for (size_t i = 0;; i++)
    {
        ssize_t cc = read(fd, buf, SZ / 2);
        if (cc < 0)
        {
            printf("%s: read bigfile failed\n", s);
            exit(1);
        }
        if (cc == 0)
        {
            break;
        }
        if (cc != SZ / 2)
        {
            printf("%s: short read bigfile\n", s);
            exit(1);
        }
        if (buf[0] != i / 2 || buf[SZ / 2 - 1] != i / 2)
        {
            printf("%s: read bigfile wrong data\n", s);
            exit(1);
        }
        total += cc;
    }
    close(fd);
    if (total != N * SZ)
    {
        printf("%s: read bigfile wrong total\n", s);
        exit(1);
    }
    unlink("bigfile.dat");
}

void fourteen(char *s)
{
    FILE_DESCRIPTOR fd;

    // XV6_NAME_MAX is 14.

    if (mkdir("12345678901234", 0755) != 0)
    {
        printf("%s: mkdir 12345678901234 failed\n", s);
        exit(1);
    }
    if (mkdir("12345678901234/123456789012345", 0755) != 0)
    {
        printf("%s: mkdir 12345678901234/123456789012345 failed\n", s);
        exit(1);
    }

    fd =
        open("123456789012345/123456789012345/123456789012345", O_CREATE, 0755);
    if (fd < 0)
    {
        printf(
            "%s: create 123456789012345/123456789012345/123456789012345 "
            "failed\n",
            s);
        exit(1);
    }
    close(fd);
    fd = open("12345678901234/12345678901234/12345678901234", 0);
    if (fd < 0)
    {
        printf("%s: open 12345678901234/12345678901234/12345678901234 failed\n",
               s);
        exit(1);
    }
    close(fd);

    if (mkdir("12345678901234/12345678901234", 0755) == 0)
    {
        printf("%s: mkdir 12345678901234/12345678901234 succeeded!\n", s);
        exit(1);
    }
    if (mkdir("123456789012345/12345678901234", 0755) == 0)
    {
        printf("%s: mkdir 12345678901234/123456789012345 succeeded!\n", s);
        exit(1);
    }

    // clean up
    unlink("123456789012345/12345678901234");
    unlink("12345678901234/12345678901234");
    unlink("12345678901234/12345678901234/12345678901234");
    unlink("123456789012345/123456789012345/123456789012345");
    unlink("12345678901234/123456789012345");
    unlink("12345678901234");
}

void rmdot(char *s)
{
    if (mkdir("dots", 0755) != 0)
    {
        printf("%s: mkdir dots failed\n", s);
        exit(1);
    }
    if (chdir("dots") != 0)
    {
        printf("%s: chdir dots failed\n", s);
        exit(1);
    }
    if (unlink(".") == 0)
    {
        printf("%s: rm . worked!\n", s);
        exit(1);
    }
    if (unlink("..") == 0)
    {
        printf("%s: rm .. worked!\n", s);
        exit(1);
    }
    if (chdir("/utests-tmp") != 0)
    {
        printf("%s: chdir / failed\n", s);
        exit(1);
    }
    if (unlink("dots/.") == 0)
    {
        printf("%s: unlink dots/. worked!\n", s);
        exit(1);
    }
    if (unlink("dots/..") == 0)
    {
        printf("%s: unlink dots/.. worked!\n", s);
        exit(1);
    }
    if (unlink("dots") != 0)
    {
        printf("%s: unlink dots failed!\n", s);
        exit(1);
    }
}

void dirfile(char *s)
{
    int fd = open("dirfile", O_CREATE, 0755);
    if (fd < 0)
    {
        printf("%s: create dirfile failed\n", s);
        exit(1);
    }
    close(fd);
    if (chdir("dirfile") == 0)
    {
        printf("%s: chdir dirfile succeeded!\n", s);
        exit(1);
    }
    fd = open("dirfile/xx", 0);
    if (fd >= 0)
    {
        printf("%s: create dirfile/xx succeeded!\n", s);
        exit(1);
    }
    fd = open("dirfile/xx", O_CREATE, 0755);
    if (fd >= 0)
    {
        printf("%s: create dirfile/xx succeeded!\n", s);
        exit(1);
    }
    if (mkdir("dirfile/xx", 0755) == 0)
    {
        printf("%s: mkdir dirfile/xx succeeded!\n", s);
        exit(1);
    }
    if (unlink("dirfile/xx") == 0)
    {
        printf("%s: unlink dirfile/xx succeeded!\n", s);
        exit(1);
    }
    if (link("/README.md", "dirfile/xx") == 0)
    {
        printf("%s: link to dirfile/xx succeeded!\n", s);
        exit(1);
    }
    if (unlink("dirfile") != 0)
    {
        printf("%s: unlink dirfile failed!\n", s);
        exit(1);
    }

    fd = open(".", O_RDWR);
    if (fd >= 0)
    {
        printf("%s: open . for writing succeeded!\n", s);
        exit(1);
    }
    fd = open(".", 0);
    if (write(fd, "x", 1) > 0)
    {
        printf("%s: write . succeeded!\n", s);
        exit(1);
    }
    close(fd);
}

// test that inode_put() is called at the end of _namei().
// also tests empty file names.
void iref(char *s)
{
    for (size_t i = 0; i < MAX_ACTIVE_INODES + 1; i++)
    {
        if (mkdir("irefd", 0755) != 0)
        {
            printf("%s: mkdir irefd failed\n", s);
            exit(1);
        }
        if (chdir("irefd") != 0)
        {
            printf("%s: chdir irefd failed\n", s);
            exit(1);
        }

        mkdir("", 0755);
        link("README", "");
        int fd = open("", O_CREATE, 0755);
        if (fd >= 0)
        {
            close(fd);
        }
        fd = open("xx", O_CREATE, 0755);
        if (fd >= 0)
        {
            close(fd);
        }
        unlink("xx");
    }

    // clean up
    for (size_t i = 0; i < MAX_ACTIVE_INODES + 1; i++)
    {
        chdir("..");
        unlink("irefd");
    }

    chdir("/utests-tmp");
}

// test that fork fails gracefully
// the forktest binary also does this, but it runs out of proc entries first.
// inside the bigger usertests binary, we run out of memory first.
void forktest(char *s)
{
    enum
    {
        N = 1000
    };
    int32_t n;

    for (n = 0; n < N; n++)
    {
        pid_t pid = fork();
        if (pid < 0) break;
        if (pid == 0) exit(0);
    }

    if (n == 0)
    {
        printf("%s: no fork at all!\n", s);
        exit(1);
    }

    if (n == N)
    {
        printf("%s: fork claimed to work 1000 times!\n", s);
        exit(1);
    }

    for (; n > 0; n--)
    {
        if (wait(NULL) < 0)
        {
            printf("%s: wait stopped early\n", s);
            exit(1);
        }
    }

    if (wait(NULL) != -1)
    {
        printf("%s: wait got too many\n", s);
        exit(1);
    }
}

void sbrkbasic(char *s)
{
    const size_t TOOMUCH = 1024 * 1024 * 1024;
    char *c, *a, *b;

    // does sbrk() return the expected failure value?
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("fork failed in sbrkbasic\n");
        exit(1);
    }
    if (pid == 0)
    {
        a = sbrk(TOOMUCH);
        if (a == (char *)TEST_POINTER_ADDR_2)
        {
            // it's OK if this fails.
            exit(0);
        }

        for (b = a; b < a + TOOMUCH; b += 4096)
        {
            *b = 99;
        }

        // we should not get here! either sbrk(TOOMUCH)
        // should have failed, or (with lazy allocation)
        // a pagefault should have killed this process.
        exit(1);
    }

    int32_t xstatus;
    wait(&xstatus);
    xstatus = WEXITSTATUS(xstatus);
    if (xstatus == 1)
    {
        printf("%s: too much memory allocated!\n", s);
        exit(1);
    }

    // can one sbrk() less than a page?
    a = sbrk(0);
    for (size_t i = 0; i < 5000; i++)
    {
        b = sbrk(1);
        if (b != a)
        {
            printf("%s: sbrk test failed %d %x %x\n", s, i, a, b);
            exit(1);
        }
        *b = 1;
        a = b + 1;
    }
    pid = fork();
    if (pid < 0)
    {
        printf("%s: sbrk test fork failed\n", s);
        exit(1);
    }
    c = sbrk(1);
    c = sbrk(1);
    if (c != a + 1)
    {
        printf("%s: sbrk test failed post-fork\n", s);
        exit(1);
    }
    if (pid == 0) exit(0);
    wait(&xstatus);
    xstatus = WEXITSTATUS(xstatus);
    exit(xstatus);
}

void sbrkmuch(char *s)
{
    // half the physical memory
    const size_t BIG = MEMORY_SIZE / 2 * 1024 * 1024;

    char *oldbrk = sbrk(0);

    // can one grow address space to something big?
    char *a = sbrk(0);
    size_t amt = BIG - (size_t)a;
    char *p = sbrk(amt);
    if (p != a)
    {
        printf(
            "%s: sbrk test failed to grow big address space; enough phys "
            "mem?\n",
            s);
        exit(1);
    }

    // touch each page to make sure it exists.
    char *eee = sbrk(0);

    long page_size = sysconf(_SC_PAGE_SIZE);
    for (char *pp = a; pp < eee; pp += page_size)
    {
        *pp = 1;
    }

    // 32bit code in release mode had gcc trigger a stringop-overflow error
    // "volatile" silences this, "-Wno-error=stringop-overflow" would work too.
    volatile char *lastaddr = (char *)(BIG - 1);
    *lastaddr = 99;

    // can one de-allocate?
    a = sbrk(0);
    char *c = sbrk(-page_size);
    if (c == (char *)TEST_POINTER_ADDR_2)
    {
        printf("%s: sbrk could not deallocate\n", s);
        exit(1);
    }
    c = sbrk(0);
    if (c != a - page_size)
    {
        printf("%s: sbrk deallocation produced wrong address, a %x c %x\n", s,
               a, c);
        exit(1);
    }

    // can one re-allocate that page?
    a = sbrk(0);
    c = sbrk(page_size);
    if (c != a || sbrk(0) != a + page_size)
    {
        printf("%s: sbrk re-allocation failed, a %x c %x\n", s, a, c);
        exit(1);
    }
    if (*lastaddr == 99)
    {
        // should be zero
        printf("%s: sbrk de-allocation didn't really deallocate\n", s);
        exit(1);
    }

    a = sbrk(0);
    c = sbrk(-((char *)sbrk(0) - oldbrk));
    if (c != a)
    {
        printf("%s: sbrk downsize failed, a %x c %x\n", s, a, c);
        exit(1);
    }
}

// can we read the kernel's memory?
void kernmem(char *s)
{
    for (char *a = (char *)(KERNBASE); a < (char *)(KERNBASE + 200000);
         a += 20000)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            printf("%s: fork failed\n", s);
            exit(1);
        }
        if (pid == 0)
        {
            printf("%s: oops could read %x = %x\n", s, a, *a);
            exit(1);
        }
        int32_t xstatus;
        wait(&xstatus);
        xstatus = WEXITSTATUS(xstatus);
        if (xstatus != -1)  // did kernel kill child?
        {
            exit(1);
        }
    }
}

// user code should not be able to write to addresses above MAXVA.
// only on 64 bit where addresses above MAXVA are possible
void MAXVAplus(char *s)
{
#ifdef _ARCH_32BIT
    return;
#else
    volatile size_t a = MAXVA;
    for (; a != 0; a <<= 1)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            printf("%s: fork failed\n", s);
            exit(1);
        }
        if (pid == 0)
        {
            *(char *)a = 99;
            printf("%s: oops wrote %x\n", s, (unsigned int)a);
            exit(1);
        }
        int32_t xstatus;
        wait(&xstatus);
        xstatus = WEXITSTATUS(xstatus);
        if (xstatus != -1)  // did kernel kill child?
        {
            exit(1);
        }
    }
#endif
}

// if we run the system out of memory, does it clean up the last
// failed allocation?
void sbrkfail(char *s)
{
    _Static_assert(MEMORY_SIZE > 4);
    const size_t BIG = (MEMORY_SIZE - 4) * 1024 * 1024;

    pid_t pids[10];

    int fds[2];
    if (pipe(fds) != 0)
    {
        printf("%s: pipe() failed\n", s);
        exit(1);
    }
    for (size_t i = 0; i < sizeof(pids) / sizeof(pids[0]); i++)
    {
        if ((pids[i] = fork()) == 0)
        {
            // allocate a lot of memory
            sbrk(BIG - (size_t)sbrk(0));
            write(fds[1], "x", 1);
            // sit around until killed
            while (true)
            {
                sleep(1000);
            }
        }
        if (pids[i] != -1)
        {
            char scratch;
            read(fds[0], &scratch, 1);
        }
    }

    // if those failed allocations freed up the pages they did allocate,
    // we'll be able to allocate here
    long page_size = sysconf(_SC_PAGE_SIZE);
    char *c = sbrk(page_size);
    for (size_t i = 0; i < sizeof(pids) / sizeof(pids[0]); i++)
    {
        if (pids[i] == -1) continue;
        kill(pids[i], SIGKILL);
        wait(NULL);
    }
    if (c == (char *)TEST_POINTER_ADDR_2)
    {
        printf("%s: failed sbrk leaked memory\n", s);
        exit(1);
    }

    // test running fork with the above allocated page
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("%s: fork failed\n", s);
        exit(1);
    }
    if (pid == 0)
    {
        // allocate a lot of memory.
        // this should produce a page fault,
        // and thus not complete.
        char *a = sbrk(0);
        sbrk(10 * BIG);
        int32_t n = 0;
        for (size_t i = 0; i < 10 * BIG; i += page_size)
        {
            n += *(a + i);
        }
        // print n so the compiler doesn't optimize away
        // the for loop.
        printf("%s: allocate a lot of memory succeeded %d\n", s, n);
        exit(1);
    }

    int32_t xstatus;
    wait(&xstatus);
    xstatus = WEXITSTATUS(xstatus);
    if (xstatus != -1 && xstatus != 2)
    {
        exit(1);
    }
}

// test reads/writes from/to allocated memory
void sbrkarg(char *s)
{
    long page_size = sysconf(_SC_PAGE_SIZE);
    char *a = sbrk(page_size);
    int fd = open("sbrk", O_CREATE | O_WRONLY, 0755);
    unlink("sbrk");
    if (fd < 0)
    {
        printf("%s: open sbrk failed\n", s);
        exit(1);
    }

    ssize_t n = write(fd, a, page_size);
    if (n < 0)
    {
        printf("%s: write sbrk failed\n", s);
        exit(1);
    }
    close(fd);

    // test writes to allocated memory
    a = sbrk(page_size);
    if (pipe((int *)a) != 0)
    {
        printf("%s: pipe() failed\n", s);
        exit(1);
    }
}

void validatetest(char *s)
{
    size_t hi = 1100 * 1024;
    long page_size = sysconf(_SC_PAGE_SIZE);

    for (size_t p = 0; p <= hi; p += page_size)
    {
        // try to crash the kernel by passing in a bad string pointer
        if (link("nosuchfile", (char *)p) != -1)
        {
            printf("%s: link should not succeed\n", s);
            exit(1);
        }
    }
}

// does uninitialized data start out zero?
char uninit[10000];
void bsstest(char *s)
{
    for (size_t i = 0; i < sizeof(uninit); i++)
    {
        if (uninit[i] != '\0')
        {
            printf("%s: bss test failed\n", s);
            exit(1);
        }
    }
}

// does execv return an error if the arguments
// are larger than a page? or does it write
// below the stack and wreck the instructions/data?
void bigargtest(char *s)
{
    unlink("bigarg-ok");

    pid_t pid = fork();
    if (pid == 0)
    {
        static char *args[MAX_EXEC_ARGS];
        for (size_t i = 0; i < MAX_EXEC_ARGS - 1; i++)
            args[i] =
                "bigargs test: failed\n                                        "
                "                                                              "
                "                                                              "
                "                                   ";
        args[MAX_EXEC_ARGS - 1] = 0;
        execv(bin_echo, args);
        int fd = open("bigarg-ok", O_CREATE, 0755);
        close(fd);
        exit(0);
    }
    else if (pid < 0)
    {
        printf("%s: bigargtest: fork failed\n", s);
        exit(1);
    }

    int32_t xstatus;
    wait(&xstatus);
    xstatus = WEXITSTATUS(xstatus);
    if (xstatus != 0)
    {
        exit(xstatus);
    }

    int fd = open("bigarg-ok", 0);
    if (fd < 0)
    {
        printf("%s: bigarg test failed!\n", s);
        exit(1);
    }
    close(fd);
}

// what happens when the file system runs out of blocks?
// answer: balloc panics, so this test is not useful.
void fsfull()
{
    int nfiles;

    printf("fsfull test\n");

    for (nfiles = 0;; nfiles++)
    {
        char name[64];
        name[0] = 'f';
        name[1] = '0' + nfiles / 1000;
        name[2] = '0' + (nfiles % 1000) / 100;
        name[3] = '0' + (nfiles % 100) / 10;
        name[4] = '0' + (nfiles % 10);
        name[5] = '\0';
        printf("writing %s\n", name);
        int fd = open(name, O_CREATE | O_RDWR, 0755);
        if (fd < 0)
        {
            printf("open %s failed\n", name);
            break;
        }
        int32_t total = 0;
        int32_t fsblocks = 0;
        while (true)
        {
            ssize_t cc = write(fd, buf, BLOCK_SIZE);
            if (cc < BLOCK_SIZE)
            {
                break;
            }
            total += cc;
            fsblocks++;
        }
        printf("wrote %d bytes\n", total);
        close(fd);
        if (total == 0) break;
    }

    while (nfiles >= 0)
    {
        char name[64];
        name[0] = 'f';
        name[1] = '0' + nfiles / 1000;
        name[2] = '0' + (nfiles % 1000) / 100;
        name[3] = '0' + (nfiles % 100) / 10;
        name[4] = '0' + (nfiles % 10);
        name[5] = '\0';
        unlink(name);
        nfiles--;
    }

    printf("fsfull test finished\n");
}

void argptest(char *s)
{
    int fd = open(bin_init, O_RDONLY);
    if (fd < 0)
    {
        printf("%s: open failed\n", s);
        exit(1);
    }
    read(fd, sbrk(0) - 1, -1);
    close(fd);
}

static inline size_t read_stack_pointer()
{
    size_t x;
    asm volatile("mv %0, sp" : "=r"(x));
    return x;
}

// check that there's an invalid page beneath
// the user stack, to catch stack overflow.
void stacktest(char *s)
{
    pid_t pid = fork();
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pid == 0)
    {
        char *sp = (char *)read_stack_pointer();
        sp -= page_size;
        // the *sp should cause a trap.
        printf("%s: stacktest: read below stack %p\n", s, *sp);
        exit(1);
    }
    else if (pid < 0)
    {
        printf("%s: fork failed\n", s);
        exit(1);
    }

    int32_t xstatus;
    wait(&xstatus);
    xstatus = WEXITSTATUS(xstatus);
    if (xstatus == -1)  // kernel killed child?
    {
        exit(0);
    }
    else
    {
        exit(xstatus);
    }
}

// check that writes to text segment fault
void textwrite(char *s)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        volatile int *addr = (int *)0;
        *addr = 10;
        exit(1);
    }
    else if (pid < 0)
    {
        printf("%s: fork failed\n", s);
        exit(1);
    }

    int32_t xstatus;
    wait(&xstatus);
    xstatus = WEXITSTATUS(xstatus);
    if (xstatus == -1)  // kernel killed child?
    {
        exit(0);
    }
    else
    {
        exit(xstatus);
    }
}

// regression test. uvm_copy_in(), uvm_copy_out(), and uvm_copy_in_str() used to
// cast the virtual page address to uint32_t, which (with certain wild system
// call arguments) resulted in a kernel page faults.
void *big = (void *)0xeaeb0b5b00002f5e;
void pgbug(char *s)
{
    char *argv[1];
    argv[0] = 0;
    execv(big, argv);
    pipe(big);

    exit(0);
}

// regression test. does the kernel panic if a process sbrk()s its
// size to be less than a page, or zero, or reduces the break by an
// amount too small to cause a page to be freed?
void sbrkbugs(char *s)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        intptr_t sz = (intptr_t)sbrk(0);
        // free all user memory; there used to be a bug that
        // would not adjust p->sz correctly in this case,
        // causing exit() to panic.
        sbrk(-sz);
        // user page fault here.
        exit(0);
    }
    wait(NULL);

    pid = fork();
    if (pid < 0)
    {
        printf("fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        intptr_t sz = (intptr_t)sbrk(0);
        // set the break to somewhere in the very first
        // page; there used to be a bug that would incorrectly
        // free the first page.
        sbrk(-(sz - 3500));
        exit(0);
    }
    wait(NULL);

    pid = fork();
    if (pid < 0)
    {
        printf("fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        long page_size = sysconf(_SC_PAGE_SIZE);
        // set the break in the middle of a page.
        size_t half_page = page_size / 2;
        sbrk((10 * page_size + half_page) - (intptr_t)sbrk(0));

        // reduce the break a bit, but not enough to
        // cause a page to be freed. this used to cause
        // a panic.
        sbrk(-10);

        exit(0);
    }
    wait(NULL);

    exit(0);
}

// if process size was somewhat more than a page boundary, and then
// shrunk to be somewhat less than that page boundary, can the kernel
// still uvm_copy_in() from addresses in the last page?
void sbrklast(char *s)
{
    intptr_t top = (intptr_t)sbrk(0);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if ((top % page_size) != 0)
    {
        sbrk(page_size - (top % page_size));
    }

    sbrk(page_size);
    sbrk(10);
    sbrk(-20);

    top = (intptr_t)sbrk(0);
    char *p = (char *)(top - 64);
    p[0] = 'x';
    p[1] = '\0';
    int fd = open(p, O_RDWR | O_CREATE, 0755);
    write(fd, p, 1);
    close(fd);
    fd = open(p, O_RDWR);
    p[0] = '\0';
    read(fd, p, 1);
    if (p[0] != 'x')
    {
        exit(1);
    }
}

// does sbrk handle signed int32 wrap-around with
// negative arguments?
void sbrk8000(char *s)
{
    sbrk(0x80000004);
    volatile char *top = sbrk(0);
    *(top - 1) = *(top - 1) + 1;
}

// regression test. test whether execv() leaks memory if one of the
// arguments is invalid. Memory leaks will get detacted at the end of the
// usertests.
void badarg(char *s)
{
    for (size_t i = 0; i < 5; i++)
    {
        char *argv[2];
        argv[0] = (char *)(-1);
        argv[1] = 0;
        execv(bin_echo, argv);
    }

    exit(0);
}

/// @brief Reads from /dev/null should return 0, writes to it should return the
/// length of the written string.
/// @param s test name
void dev_null(char *s)
{
    const size_t N = 3;
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0)
    {
        printf("%s: error: could not open /dev/null\n", s);
        exit(1);
    }
    for (size_t i = 0; i < N; i++)
    {
        size_t len = 1 + i;
        ssize_t r = write(fd, "aaaaaaaaaa", len);
        if (r != len)
        {
            printf("%s: error: write to /dev/null failed\n", s);
            exit(1);
        }
        r = read(fd, buf, len);
        if (r != 0)
        {
            printf("%s: read of /dev/null should return 0\n", s);
            exit(1);
        }
    }
    close(fd);
}

/// @brief Reads from /dev/zero should fill the buffer with '0', writes to it
/// should return the length of the written string.
/// @param s test name
void dev_zero(char *s)
{
    const size_t N = 4;
    int fd = open("/dev/zero", O_RDWR);
    if (fd < 0)
    {
        printf("%s: error: could not open /dev/zero\n", s);
        exit(1);
    }
    for (size_t i = 0; i < N; i++)
    {
        size_t len = 1 + i;
        ssize_t r = write(fd, "aaaaaaaaaa", len);
        if (r != len)
        {
            printf("%s: error: write to /dev/zero failed\n", s);
            exit(1);
        }

        if (i == N - 1)
        {
            len = 5000;  // > 1 PAGE_SIZE
        }
        for (size_t j = 0; j < len; ++j)
        {
            buf[j] = 0xFF;
        }
        r = read(fd, buf, len);
        if (r != len)
        {
            printf("%s: read of /dev/zero failed\n", s);
            exit(1);
        }
        for (size_t j = 0; j < len; ++j)
        {
            if (buf[j] != 0)
            {
                printf("%s: read of /dev/zero did not return 0 at pos %ld\n", s,
                       j);
                exit(1);
            }
        }
    }
    close(fd);
}

struct test
{
    void (*f)(char *);
    char *s;
} quicktests[] = {
    {copyin, "copyin"},
    {copyout, "copyout"},
    {copyinstr1, "copyinstr1"},
    {copyinstr2, "copyinstr2"},
    {copyinstr3, "copyinstr3"},
    {rwsbrk, "rwsbrk"},
    {truncate1, "truncate1"},
    {truncate2, "truncate2"},
    {truncate3, "truncate3"},
    {openiputtest, "openiput"},
    {exitiputtest, "exitiput"},
    {iputtest, "iput"},
    {opentest, "opentest"},
    {writetest, "writetest"},
    {writebig, "writebig"},
    {createtest, "createtest"},
    {dirtest, "dirtest"},
    {exectest, "exectest"},
    {pipe1, "pipe1"},
    {killstatus, "killstatus"},
    {preempt, "preempt"},
    {exitwait, "exitwait"},
    {reparent, "reparent"},
    {twochildren, "twochildren"},
    {forkfork, "forkfork"},
    {forkforkfork, "forkforkfork"},
    {reparent2, "reparent2"},
    {mem, "mem"},
    {sharedfd, "sharedfd"},
    {fourfiles, "fourfiles"},
    {createdelete, "createdelete"},
    {unlinkread, "unlinkread"},
    {linktest, "linktest"},
    {concreate, "concreate"},
    {linkunlink, "linkunlink"},
    {subdir, "subdir"},
    {bigwrite, "bigwrite"},
    {bigfile, "bigfile"},
    {fourteen, "fourteen"},
    {rmdot, "rmdot"},
    {dirfile, "dirfile"},
    {iref, "iref"},
    {forktest, "forktest"},
    {sbrkbasic, "sbrkbasic"},
    {sbrkmuch, "sbrkmuch"},
    {kernmem, "kernmem"},
    {MAXVAplus, "MAXVAplus"},
    {sbrkfail, "sbrkfail"},
    {sbrkarg, "sbrkarg"},
    {validatetest, "validatetest"},
    {bsstest, "bsstest"},
    {bigargtest, "bigargtest"},
    {argptest, "argptest"},
    {stacktest, "stacktest"},
    {textwrite, "textwrite"},
    {pgbug, "pgbug"},
    {sbrkbugs, "sbrkbugs"},
    {sbrklast, "sbrklast"},
    {sbrk8000, "sbrk8000"},
    {badarg, "badarg"},
    {dev_null, "dev_null"},
    {dev_zero, "dev_zero"},

    {0, 0},
};

//
// Section with tests that take a fair bit of time
//

// directory that uses indirect blocks
void bigdir(char *s)
{
    const size_t N = 500;
    int32_t i, fd;
    char name[10];

    unlink("bd");

    fd = open("bd", O_CREATE);
    if (fd < 0)
    {
        printf("%s: bigdir create failed\n", s);
        exit(1);
    }
    close(fd);

    for (i = 0; i < N; i++)
    {
        name[0] = 'x';
        name[1] = '0' + (i / 64);
        name[2] = '0' + (i % 64);
        name[3] = '\0';
        if (link("bd", name) != 0)
        {
            printf("%s: bigdir link(bd, %s) failed\n", s, name);
            exit(1);
        }
    }

    unlink("bd");
    for (i = 0; i < N; i++)
    {
        name[0] = 'x';
        name[1] = '0' + (i / 64);
        name[2] = '0' + (i % 64);
        name[3] = '\0';
        if (unlink(name) != 0)
        {
            printf("%s: bigdir unlink failed", s);
            exit(1);
        }
    }
}

// concurrent writes to try to provoke deadlock in the virtio disk
// driver.
void manywrites(char *s)
{
    size_t nchildren = 4;
    int32_t howmany = 30;

    for (size_t ci = 0; ci < nchildren; ci++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            printf("fork failed\n");
            exit(1);
        }

        if (pid == 0)
        {
            char name[3];
            name[0] = 'b';
            name[1] = 'a' + ci;
            name[2] = '\0';
            unlink(name);

            for (size_t iters = 0; iters < howmany; iters++)
            {
                for (size_t i = 0; i < ci + 1; i++)
                {
                    int fd = open(name, O_CREATE | O_RDWR, 0755);
                    if (fd < 0)
                    {
                        printf("%s: cannot create %s\n", s, name);
                        exit(1);
                    }
                    ssize_t sz = sizeof(buf);
                    ssize_t cc = write(fd, buf, sz);
                    if (cc != sz)
                    {
                        printf("%s: write(%ld) ret %ld\n", s, sz, cc);
                        exit(1);
                    }
                    close(fd);
                }
                unlink(name);
            }

            unlink(name);
            exit(0);
        }
    }

    for (size_t ci = 0; ci < nchildren; ci++)
    {
        int32_t st = 0;
        wait(&st);
        st = WEXITSTATUS(st);
        if (st != 0)
        {
            exit(st);
        }
    }
    exit(0);
}

// regression test. does write() with an invalid buffer pointer cause
// a block to be allocated for a file that is then not freed when the
// file is deleted? if the kernel has this bug, it will panic: balloc:
// out of blocks. assumed_free may need to be raised to be more than
// the number of free blocks. this test takes a long time.
void badwrite(char *s)
{
    int assumed_free = 600;

    unlink("junk");
    for (size_t i = 0; i < assumed_free; i++)
    {
        int fd = open("junk", O_CREATE | O_WRONLY, 0755);
        if (fd < 0)
        {
            printf("open junk failed\n");
            exit(1);
        }
        write(fd, (char *)0xffffffffffL, 1);
        close(fd);
        unlink("junk");
    }

    int fd = open("junk", O_CREATE | O_WRONLY, 0755);
    if (fd < 0)
    {
        printf("open junk failed\n");
        exit(1);
    }
    if (write(fd, "x", 1) != 1)
    {
        printf("write failed\n");
        exit(1);
    }
    close(fd);
    unlink("junk");

    exit(0);
}

// test the execv() code that cleans up if it runs out
// of memory. it's really a test that such a condition
// doesn't cause a panic.
void execout(char *s)
{
    for (size_t avail = 0; avail < 15; avail++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            printf("fork failed\n");
            exit(1);
        }
        else if (pid == 0)
        {
            long page_size = sysconf(_SC_PAGE_SIZE);
            // allocate all of memory.
            while (true)
            {
                intptr_t a = (intptr_t)sbrk(page_size);
                if (a == TEST_POINTER_ADDR_2)
                {
                    break;
                }
                *(char *)(a + page_size - 1) = 1;
            }

            // free a few pages, in order to let execv() make some
            // progress.
            for (size_t i = 0; i < avail; i++)
            {
                sbrk(-page_size);
            }

            close(1);
            char *args[] = {"echo", "x", 0};
            execv(bin_echo, args);
            exit(0);
        }
        else
        {
            wait(NULL);
        }
    }

    exit(0);
}

// can the kernel tolerate running out of disk space?
void diskfull(char *s)
{
    unlink("diskfulldir");

    bool done = false;
    size_t fi = 0;
    for (fi = 0; done == false; fi++)
    {
        char name[32];
        name[0] = 'b';
        name[1] = 'i';
        name[2] = 'g';
        name[3] = '0' + fi;
        name[4] = '\0';
        unlink(name);
        int fd = open(name, O_CREATE | O_RDWR | O_TRUNC, 0755);
        if (fd < 0)
        {
            // oops, ran out of inodes before running out of blocks.
            printf("%s: could not create file %s\n", s, name);
            done = true;
            break;
        }
        for (size_t i = 0; i < MAXFILE; i++)
        {
            char buf[BLOCK_SIZE];
            if (write(fd, buf, BLOCK_SIZE) != BLOCK_SIZE)
            {
                done = true;
                close(fd);
                break;
            }
        }
        close(fd);
    }

    // now that there are no free blocks, test that inode_dir_link()
    // merely fails (doesn't panic) if it can't extend
    // directory content. one of these file creations
    // is expected to fail.
    size_t nzz = 128;
    for (size_t i = 0; i < nzz; i++)
    {
        char name[32];
        name[0] = 'z';
        name[1] = 'z';
        name[2] = '0' + (i / 32);
        name[3] = '0' + (i % 32);
        name[4] = '\0';
        unlink(name);
        int fd = open(name, O_CREATE | O_RDWR | O_TRUNC, 0755);
        if (fd < 0) break;
        close(fd);
    }

    // this mkdir() is expected to fail.
    if (mkdir("diskfulldir", 0755) == 0)
    {
        printf("%s: mkdir(diskfulldir) unexpectedly succeeded!\n", s);
    }

    unlink("diskfulldir");

    for (size_t i = 0; i < nzz; i++)
    {
        char name[32];
        name[0] = 'z';
        name[1] = 'z';
        name[2] = '0' + (i / 32);
        name[3] = '0' + (i % 32);
        name[4] = '\0';
        unlink(name);
    }

    for (size_t i = 0; i < fi; i++)
    {
        char name[32];
        name[0] = 'b';
        name[1] = 'i';
        name[2] = 'g';
        name[3] = '0' + i;
        name[4] = '\0';
        unlink(name);
    }
}

void outofinodes(char *s)
{
    size_t nzz = 32 * 32;
    for (size_t i = 0; i < nzz; i++)
    {
        char name[32];
        name[0] = 'z';
        name[1] = 'z';
        name[2] = '0' + (i / 32);
        name[3] = '0' + (i % 32);
        name[4] = '\0';
        unlink(name);
        int fd = open(name, O_CREATE | O_RDWR | O_TRUNC, 0755);
        if (fd < 0)
        {
            // failure is eventually expected.
            break;
        }
        close(fd);
    }

    for (size_t i = 0; i < nzz; i++)
    {
        char name[32];
        name[0] = 'z';
        name[1] = 'z';
        name[2] = '0' + (i / 32);
        name[3] = '0' + (i % 32);
        name[4] = '\0';
        unlink(name);
    }
}

struct test slowtests[] = {
    {bigdir, "bigdir"},
    {manywrites, "manywrites"},
    {badwrite, "badwrite"},
    {execout, "execout"},
    {diskfull, "diskfull"},
    {outofinodes, "outofinodes"},

    {0, 0},
};

//
// drive tests
//

// run each test in its own process. run returns 1 if child's exit()
// indicates success.
int run(void f(char *), char *s)
{
    printf("test %s: ", s);
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("runtest: fork error\n");
        exit(1);
    }
    if (pid == 0)
    {
        f(s);
        exit(0);
    }
    else
    {
        int32_t xstatus;
        wait(&xstatus);
        xstatus = WEXITSTATUS(xstatus);
        if (xstatus != 0)
        {
            printf("FAILED\n");
        }
        else
        {
            printf("OK\n");
        }
        return xstatus == 0;
    }
}

int runtests(struct test *tests, char *justone)
{
    for (struct test *t = tests; t->s != 0; t++)
    {
        if ((justone == 0) || strcmp(t->s, justone) == 0)
        {
            if (!run(t->f, t->s))
            {
                printf("SOME TESTS FAILED\n");
                return 1;
            }
        }
    }
    return 0;
}

//
// use sbrk() to count how many free physical memory pages there are.
// touches the pages to force allocation.
// because out of memory with lazy allocation results in the process
// taking a fault and being killed, fork and report back.
//
int countfree()
{
    int fds[2];

    if (pipe(fds) < 0)
    {
        printf("pipe() failed in countfree()\n");
        exit(1);
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        printf("fork failed in countfree()\n");
        exit(1);
    }

    if (pid == 0)
    {
        close(fds[0]);
        long page_size = sysconf(_SC_PAGE_SIZE);

        while (true)
        {
            void *a = sbrk(page_size);
            if (a == ((void *)-1))
            {
                break;
            }

            // modify the memory to make sure it's really allocated.
            *(char *)(a + page_size - 1) = 1;

            // report back one more page.
            if (write(fds[1], "x", 1) != 1)
            {
                printf("write() failed in countfree()\n");
                exit(1);
            }
        }

        exit(0);
    }

    close(fds[1]);

    int32_t n = 0;
    while (true)
    {
        char c;
        ssize_t cc = read(fds[0], &c, 1);
        if (cc < 0)
        {
            printf("read() failed in countfree()\n");
            exit(1);
        }
        if (cc == 0)
        {
            break;
        }
        n += 1;
    }

    close(fds[0]);
    wait(NULL);

    return n;
}

int drivetests(int quick, int continuous, char *justone)
{
    mkdir("/utests-tmp", 0755);
    if (chdir("/utests-tmp") < 0) return -1;

    do {
        printf("usertests starting\n");
        int free0 = countfree();
        if (runtests(quicktests, justone))
        {
            if (continuous != 2)
            {
                return 1;
            }
        }
        if (!quick)
        {
            if (justone == 0) printf("usertests slow tests starting\n");
            if (runtests(slowtests, justone))
            {
                if (continuous != 2)
                {
                    return 1;
                }
            }
        }
        int free1 = countfree();
        if (free1 < free0)
        {
            printf("FAILED -- lost some free pages %d (out of %d)\n", free1,
                   free0);
            printf("badarg is a candidate for leaked memory\n");
            if (continuous != 2)
            {
                return 1;
            }
        }
    } while (continuous);

    if (chdir("..") < 0) return -1;
    return 0;
}

int main(int argc, char *argv[])
{
    int continuous = 0;
    bool quick_tests_only = false;
    char *justone = NULL;

    if (argc == 2 && strcmp(argv[1], "-q") == 0)
    {
        quick_tests_only = true;
    }
    else if (argc == 2 && strcmp(argv[1], "-c") == 0)
    {
        continuous = 1;
    }
    else if (argc == 2 && strcmp(argv[1], "-C") == 0)
    {
        continuous = 2;
    }
    else if (argc == 2 && argv[1][0] != '-')
    {
        justone = argv[1];
    }
    else if (argc > 1)
    {
        printf("Usage: usertests [-c] [-C] [-q] [testname]\n");
        return 1;
    }
    if (drivetests(quick_tests_only, continuous, justone))
    {
        return 1;
    }
    printf("ALL TESTS PASSED\n");

    return 0;
}
