/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <kernel/vimixfs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <vimixutils/time.h>

#define BUFSZ (32 * 1024)
char buf[BUFSZ];

const size_t file_sizes[] = {
    // 1,
    // 256,
    // 1024,
    // 4096,
    // 32 * 1024,
    128 * 1024, 256 * 1024};
const size_t num_fs_sizes = sizeof(file_sizes) / sizeof(file_sizes[0]);

// const size_t bytes_per_run[] = {1, 4, 16, 64, 256, 1024, 4096, 16 * 1024};
const size_t bytes_per_run[] = {1024, 4096, 16 * 1024, 32 * 1024};
const size_t num_bytes_per_run =
    sizeof(bytes_per_run) / sizeof(bytes_per_run[0]);

struct test
{
    size_t file_size;
    size_t bytes_per_run;
    uint64_t result_ms;
};

uint64_t bench_file_write(struct test *test)
{
    uint64_t t0 = get_time_ms();

    unlink("bigfile.dat");
    int fd = open("bigfile.dat", O_CREAT | O_RDWR, 0755);
    if (fd < 0)
    {
        printf("cannot create bigfile");
        exit(EXIT_FAILURE);
    }
    ssize_t total = 0;
    while (total < test->file_size)
    {
        ssize_t to_write = test->bytes_per_run;
        if (total + to_write > test->file_size)
        {
            to_write = test->file_size - total;
        }
        ssize_t written = write(fd, buf, to_write);

        if (written < 0)
        {
            printf("write bigfile failed\n");
            printf(
                "test: file size: %zu, byte_per_op: %zu, bytes this write: "
                "%zu\n",
                test->file_size, test->bytes_per_run, to_write);
            printf("errno: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        total += written;
    }
    close(fd);

    uint64_t t1 = get_time_ms();

    test->result_ms = t1 - t0;
    return t1 - t0;
}

uint64_t bench_file_read(struct test *test)
{
    uint64_t t0 = get_time_ms();
    int fd = open("bigfile.dat", O_RDONLY);
    if (fd < 0)
    {
        printf("cannot open bigfile\n");
        exit(EXIT_FAILURE);
    }

    ssize_t total = 0;
    while (total < test->file_size)
    {
        ssize_t to_read = test->bytes_per_run;
        if (total + to_read > test->file_size)
        {
            to_read = test->file_size - total;
        }
        ssize_t bytes_read = read(fd, buf, to_read);
        if (bytes_read < 0)
        {
            printf("read %zd bytes from bigfile failed: %s\n", to_read,
                   strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (bytes_read == 0)
        {
            break;
        }
        total += bytes_read;
    }
    close(fd);

    uint64_t t1 = get_time_ms();
    test->result_ms = t1 - t0;

    unlink("bigfile.dat");
    return t1 - t0;
}

void print_results(struct test tests[num_bytes_per_run][num_fs_sizes])
{
    printf("bytes:");
    for (size_t b = 0; b < num_bytes_per_run; b++)
    {
        printf("\t%6zu", bytes_per_run[b]);
    }
    printf("\n");

    for (size_t fsize = 0; fsize < num_fs_sizes; fsize++)
    {
        printf("%6zu", file_sizes[fsize]);
        for (size_t b = 0; b < num_bytes_per_run; b++)
        {
            if (tests[b][fsize].result_ms == (uint64_t)-1)
            {
                printf("\t   n/a");
            }
            else
            {
                printf("\t%6llu",
                       (unsigned long long)tests[b][fsize].result_ms);
            }
        }
        printf("\n");
    }
}

int main(int argc, char *argv[])
{
    struct test bench_write[num_bytes_per_run][num_fs_sizes];
    struct test bench_read[num_bytes_per_run][num_fs_sizes];

    for (size_t fsize = 0; fsize < num_fs_sizes; fsize++)
    {
        for (size_t b = 0; b < num_bytes_per_run; b++)
        {
            bench_write[b][fsize].file_size = file_sizes[fsize];
            bench_write[b][fsize].bytes_per_run = bytes_per_run[b];
            bench_write[b][fsize].result_ms = (uint64_t)-1;

            bench_read[b][fsize] = bench_write[b][fsize];

            bench_file_write(&bench_write[b][fsize]);
            bench_file_read(&bench_read[b][fsize]);
        }
    }

    printf("write results (time in ms):\n");
    print_results(bench_write);

    printf("read results (time in ms):\n");
    print_results(bench_read);

    return 0;
}
