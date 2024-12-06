/* SPDX-License-Identifier: MIT */

//
// run random system calls in parallel forever.
//

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

const char *bin_echo = "/usr/bin/echo";
const char *bin_cat = "/usr/bin/cat";

// from FreeBSD.
int do_rand(unsigned long *ctx)
{
    /*
     * Compute x = (7^5 * x) mod (2^31 - 1)
     * without overflowing 31 bits:
     *      (2^31 - 1) = 127773 * (7^5) + 2836
     * From "Random number generators: good ones are hard to find",
     * Park and Miller, Communications of the ACM, vol. 31, no. 10,
     * October 1988, p. 1195.
     */
    long hi, lo, x;

    /* Transform to [1, 0x7ffffffe] range. */
    x = (*ctx % 0x7ffffffe) + 1;
    hi = x / 127773;
    lo = x % 127773;
    x = 16807 * lo - 2836 * hi;
    if (x < 0) x += 0x7fffffff;
    /* Transform to [0, 0x7ffffffd] range. */
    x--;
    *ctx = x;
    return (x);
}

unsigned long rand_next = 1;

int rand() { return (do_rand(&rand_next)); }

void go(int which_child, int max_iterations)
{
    int fd = -1;
    static char buf[999];
    char *break0 = sbrk(0);

    mkdir("grindir", 0755);
    if (chdir("grindir") != 0)
    {
        printf("grind: chdir grindir failed\n");
        exit(1);
    }
    chdir("/");

    size_t iters = 0;
    while (iters < max_iterations)
    {
        // print child number (as letters) every few iterations
        char name = 'A' + which_child;
        if ((iters % 500) == 0) write(1, &name, 1);
        iters++;

        int what = rand() % 23;
        if (what == 1)
        {
            close(open("grindir/../a", O_CREAT | O_RDWR, 0755));
        }
        else if (what == 2)
        {
            close(open("grindir/../grindir/../b", O_CREAT | O_RDWR, 0755));
        }
        else if (what == 3)
        {
            unlink("grindir/../a");
        }
        else if (what == 4)
        {
            if (chdir("grindir") != 0)
            {
                printf("grind: chdir grindir failed\n");
                exit(1);
            }
            unlink("../b");
            chdir("/");
        }
        else if (what == 5)
        {
            close(fd);
            fd = open("/grindir/../a", O_CREAT | O_RDWR, 0755);
        }
        else if (what == 6)
        {
            close(fd);
            fd = open("/./grindir/./../b", O_CREAT | O_RDWR, 0755);
        }
        else if (what == 7)
        {
            write(fd, buf, sizeof(buf));
        }
        else if (what == 8)
        {
            read(fd, buf, sizeof(buf));
        }
        else if (what == 9)
        {
            mkdir("grindir/../a", 0755);
            close(open("a/../a/./a", O_CREAT | O_RDWR, 0755));
            unlink("a/a");
        }
        else if (what == 10)
        {
            mkdir("/../b", 0755);
            close(open("grindir/../b/b", O_CREAT | O_RDWR, 0755));
            rmdir("b/b");
        }
        else if (what == 11)
        {
            unlink("b");
            link("../grindir/./../a", "../b");
        }
        else if (what == 12)
        {
            unlink("../grindir/../a");
            link(".././b", "/grindir/../a");
        }
        else if (what == 13)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                exit(0);
            }
            else if (pid < 0)
            {
                printf("grind: fork failed\n");
                exit(1);
            }
            wait(NULL);
        }
        else if (what == 14)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                fork();
                fork();
                exit(0);
            }
            else if (pid < 0)
            {
                printf("grind: fork failed\n");
                exit(1);
            }
            wait(NULL);
        }
        else if (what == 15)
        {
            sbrk(6011);
        }
        else if (what == 16)
        {
            if ((char *)sbrk(0) > break0)
            {
                sbrk(-((char *)sbrk(0) - break0));
            }
        }
        else if (what == 17)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                close(open("a", O_CREAT | O_RDWR, 0755));
                exit(0);
            }
            else if (pid < 0)
            {
                printf("grind: fork failed\n");
                exit(1);
            }
            if (chdir("../grindir/..") != 0)
            {
                printf("grind: chdir failed\n");
                exit(1);
            }
            kill(pid, SIGKILL);
            wait(NULL);
        }
        else if (what == 18)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                kill(getpid(), SIGKILL);
                exit(0);
            }
            else if (pid < 0)
            {
                printf("grind: fork failed\n");
                exit(1);
            }
            wait(NULL);
        }
        else if (what == 19)
        {
            int fds[2];
            if (pipe(fds) < 0)
            {
                printf("grind: pipe failed\n");
                exit(1);
            }
            pid_t pid = fork();
            if (pid == 0)
            {
                fork();
                fork();
                if (write(fds[1], "x", 1) != 1)
                    printf("grind: pipe write failed\n");
                char c;
                if (read(fds[0], &c, 1) != 1)
                    printf("grind: pipe read failed\n");
                exit(0);
            }
            else if (pid < 0)
            {
                printf("grind: fork failed\n");
                exit(1);
            }
            close(fds[0]);
            close(fds[1]);
            wait(NULL);
        }
        else if (what == 20)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                unlink("a");
                mkdir("a", 0755);
                chdir("a");
                rmdir("../a");
                fd = open("x", O_CREAT | O_RDWR, 0755);
                unlink("x");
                exit(0);
            }
            else if (pid < 0)
            {
                printf("grind: fork failed\n");
                exit(1);
            }
            wait(NULL);
        }
        else if (what == 21)
        {
            unlink("c");
            // should always succeed. check that there are free i-nodes,
            // file descriptors, blocks.
            int fd1 = open("c", O_CREAT | O_RDWR, 0755);
            if (fd1 < 0)
            {
                printf("grind: create c failed\n");
                exit(1);
            }
            if (write(fd1, "x", 1) != 1)
            {
                printf("grind: write c failed\n");
                exit(1);
            }
            struct stat st;
            if (fstat(fd1, &st) != 0)
            {
                printf("grind: fstat failed\n");
                exit(1);
            }
            if (st.st_size != 1)
            {
                printf("grind: fstat reports wrong size %d\n", (int)st.st_size);
                exit(1);
            }
            if (st.st_ino > 200)
            {
                printf("grind: fstat reports crazy i-number %d\n",
                       (int)st.st_ino);
                exit(1);
            }
            close(fd1);
            unlink("c");
        }
        else if (what == 22)
        {
            // echo hi | cat
            int aa[2], bb[2];
            if (pipe(aa) < 0)
            {
                fprintf(stderr, "grind: pipe failed\n");
                exit(1);
            }
            if (pipe(bb) < 0)
            {
                fprintf(stderr, "grind: pipe failed\n");
                exit(1);
            }
            pid_t pid1 = fork();
            if (pid1 == 0)
            {
                close(bb[0]);
                close(bb[1]);
                close(aa[0]);
                close(1);
                if (dup(aa[1]) != 1)
                {
                    fprintf(stderr, "grind: dup failed\n");
                    exit(1);
                }
                close(aa[1]);
                char *args[3] = {"echo", "hi", 0};
                execv(bin_echo, args);
                fprintf(stderr, "grind: echo: not found\n");
                exit(2);
            }
            else if (pid1 < 0)
            {
                fprintf(stderr, "grind: fork failed\n");
                exit(3);
            }
            pid_t pid2 = fork();
            if (pid2 == 0)
            {
                close(aa[1]);
                close(bb[0]);
                close(0);
                if (dup(aa[0]) != 0)
                {
                    fprintf(stderr, "grind: dup failed\n");
                    exit(4);
                }
                close(aa[0]);
                close(1);
                if (dup(bb[1]) != 1)
                {
                    fprintf(stderr, "grind: dup failed\n");
                    exit(5);
                }
                close(bb[1]);
                char *args[2] = {"cat", 0};
                execv(bin_cat, args);
                fprintf(stderr, "grind: cat: not found\n");
                exit(6);
            }
            else if (pid2 < 0)
            {
                fprintf(stderr, "grind: fork failed\n");
                exit(7);
            }
            close(aa[0]);
            close(aa[1]);
            close(bb[1]);
            char buf[4] = {0, 0, 0, 0};
            read(bb[0], buf + 0, 1);
            read(bb[0], buf + 1, 1);
            read(bb[0], buf + 2, 1);
            close(bb[0]);
            int st1, st2;
            wait(&st1);
            st1 = WEXITSTATUS(st1);
            wait(&st2);
            st2 = WEXITSTATUS(st2);
            if (st1 != 0 || st2 != 0 || strcmp(buf, "hi\n") != 0)
            {
                printf("grind: execv pipeline failed %d %d \"%s\"\n", st1, st2,
                       buf);
                exit(1);
            }
        }
    }
}

void iter(size_t number_of_forks, int max_iterations)
{
    unlink("a");
    unlink("b");

    pid_t children[number_of_forks];
    for (size_t i = 0; i < number_of_forks; ++i)
    {
        children[i] = fork();
        if (children[i] < 0)
        {
            printf("grind: fork %zd failed\n", i);
            exit(1);
        }
        if (children[i] == 0)
        {
            rand_next ^= (31 * i);
            go(i, max_iterations);
            exit(0);
        }
    }

    for (size_t i = 0; i < number_of_forks; ++i)
    {
        int st1 = -1;
        wait(&st1);
        st1 = WEXITSTATUS(st1);
        children[i] = -1;

        if (st1 != 0)
        {
            for (size_t i = 0; i < number_of_forks; ++i)
            {
                if (children[i] != -1) kill(children[i], SIGKILL);
            }
        }
    }

    printf("\ngrind passed\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    size_t forks = 2;
    int max_iterations = 1000;
    if (argc == 2)
    {
        forks = atoi(argv[1]);
    }
    else if (argc == 3)
    {
        forks = atoi(argv[1]);
        max_iterations = atoi(argv[2]);
    }
    if (forks > 8)
    {
        printf("Warning: too many processes requested.\n");
        return -1;
    }
    if (forks < 2)
    {
        printf("Not enough forks requested to be a useful test.\n");
        return -1;
    }
    pid_t pid = fork();
    if (pid == 0)
    {
        iter(forks, max_iterations);
        exit(0);
    }
    if (pid > 0)
    {
        wait(NULL);
    }

    return 0;
}
