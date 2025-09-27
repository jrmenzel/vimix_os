/* SPDX-License-Identifier: MIT */

#include <ctype.h>
#include <vimixutils/minmax.h>
#include "usertests.h"

#if defined(BUILD_ON_HOST)
// we are not the only process on the host, so comparing memory isn't useful
size_t countfree() { return 0; }

struct test quicktests[] = {
    {0, 0},
};

struct test slowtests[] = {
    {0, 0},
};
#endif

char buf[BUFSZ];

/// @brief Reads from /dev/null should return 0, writes to it should return the
/// length of the written string.
/// @param s test name
void dev_null(char *s)
{
    const size_t N = 3;
    int fd = open("/dev/null", O_RDWR);
    assert_open_ok_fd(s, fd, "/dev/null");

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

/// @brief Reads from /dev/zero should fill the buffer with 0, writes to it
/// should return the length of the written string.
/// @param s test name
void dev_zero(char *s)
{
    const size_t N = 4;
    int fd = open("/dev/zero", O_RDWR);
    assert_open_ok_fd(s, fd, "/dev/zero");

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
                printf("%s: read of /dev/zero did not return 0 at pos %zd\n", s,
                       j);
                exit(1);
            }
        }
    }
    close(fd);
}

/// @brief Test lseek syscall
void lseek_test(char *s)
{
    const char *file_name = "seektest";
    int fd = open(file_name, O_CREAT | O_RDWR, 0755);
    assert_open_ok_fd(s, fd, file_name);

    // inital seek pos == 0
    off_t seek_pos = lseek(fd, 0, SEEK_CUR);
    assert_null_s(s, seek_pos);

    const char *test_str_1 = "abcdefghij";
    const char *test_str_2 = "0123456789";
    size_t expected_pos = 0;

    assert_write_to_file(s, fd, test_str_1);
    expected_pos += strlen(test_str_1);
    seek_pos = lseek(fd, 0, SEEK_CUR);
    assert_same_value(seek_pos, expected_pos);

    assert_write_to_file(s, fd, test_str_2);
    expected_pos += strlen(test_str_2);
    seek_pos = lseek(fd, 0, SEEK_CUR);
    assert_same_value(seek_pos, expected_pos);

    // set seek pos
    // from beginning of file:
    for (size_t i = 0; i < strlen(test_str_1); ++i)
    {
        seek_pos = lseek(fd, i, SEEK_SET);
        assert_same_value(seek_pos, i);

        ssize_t read_bytes = read(fd, buf, 1);
        assert_same_value(read_bytes, 1);
        assert_same_value(buf[0], test_str_1[i]);
    }

    // from end of file:
    for (size_t i = 0; i < strlen(test_str_2) - 1; ++i)
    {
        off_t off = -(i + 1);
        seek_pos = lseek(fd, off, SEEK_END);
        assert_same_value(seek_pos, 20 + off);

        ssize_t read_bytes = read(fd, buf, 1);
        assert_same_value(read_bytes, 1);
        assert_same_value(buf[0], test_str_2[strlen(test_str_2) + off]);
    }

    seek_pos = lseek(fd, 0, SEEK_SET);
    assert_same_value(seek_pos, 0);
    off_t pos = 5;
    seek_pos = lseek(fd, pos, SEEK_CUR);
    assert_same_value(seek_pos, pos);

    ssize_t read_bytes = read(fd, buf, 1);
    assert_same_value(read_bytes, 1);
    assert_same_value(buf[0], test_str_1[pos]);
    pos++;

    // forward
    seek_pos = lseek(fd, 3, SEEK_CUR);
    pos += 3;
    assert_same_value(seek_pos, pos);

    read_bytes = read(fd, buf, 1);
    assert_same_value(read_bytes, 1);
    assert_same_value(buf[0], test_str_1[pos]);
    pos++;

    // backward
    seek_pos = lseek(fd, -6, SEEK_CUR);
    pos -= 6;
    assert_same_value(seek_pos, pos);

    read_bytes = read(fd, buf, 1);
    assert_same_value(read_bytes, 1);
    assert_same_value(buf[0], test_str_1[pos]);
    pos++;

    close(fd);
}

char ctype_results_isprint[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char ctype_results_iscntrl[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char ctype_results_isalnum[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char ctype_results_isalpha[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char ctype_results_isdigit[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char ctype_results_isgraph[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char ctype_results_islower[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char ctype_results_isupper[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char ctype_results_ispunct[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char ctype_results_isspace[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char ctype_results_isxdigit[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

unsigned char ctype_results_tolower[256] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
    30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
    45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
    60,  61,  62,  63,  64,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106,
    107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
    122, 91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104,
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
    135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
    165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
    225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
    255};

unsigned char ctype_results_toupper[256] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
    30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
    45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
    60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
    75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
    90,  91,  92,  93,  94,  95,  96,  65,  66,  67,  68,  69,  70,  71,  72,
    73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,
    88,  89,  90,  123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
    135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
    165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
    225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
    255};

void ctype_test(char *s)
{
    for (size_t i = 0; i < 256; ++i)
    {
        int res = min(isprint(i), 1);
        assert_same_value(res, ctype_results_isprint[i]);

        res = min(iscntrl(i), 1);
        assert_same_value(res, ctype_results_iscntrl[i]);

        res = min(isalnum(i), 1);
        assert_same_value(res, ctype_results_isalnum[i]);

        res = min(isalpha(i), 1);
        assert_same_value(res, ctype_results_isalpha[i]);

        res = min(isdigit(i), 1);
        assert_same_value(res, ctype_results_isdigit[i]);

        res = min(isgraph(i), 1);
        assert_same_value(res, ctype_results_isgraph[i]);

        res = min(islower(i), 1);
        assert_same_value(res, ctype_results_islower[i]);

        res = min(isupper(i), 1);
        assert_same_value(res, ctype_results_isupper[i]);

        res = min(ispunct(i), 1);
        assert_same_value(res, ctype_results_ispunct[i]);

        res = min(isspace(i), 1);
        assert_same_value(res, ctype_results_isspace[i]);

        res = min(isxdigit(i), 1);
        assert_same_value(res, ctype_results_isxdigit[i]);

        res = tolower(i);
        assert_same_value(res, ctype_results_tolower[i]);

        res = toupper(i);
        assert_same_value(res, ctype_results_toupper[i]);
    }
}

void printf_test(char *s)
{
#define MAX_STRING 128
    char buf[MAX_STRING];
    int ret;

    ret = snprintf(buf, MAX_STRING, "Test printf formatting\n");
    assert_same_string(buf, "Test printf formatting\n");
    assert_same_value(ret, 23);

    // test truncation
    assert_same_string(s, "printf");
    ret = snprintf(buf, 1, "%s", s);
    assert_same_string(buf, "");
    assert_same_value(ret, 6);  // untruncated length should be returned!

    ret = snprintf(buf, 4, "%s", s);
    assert_same_string(buf, "pri");
    assert_same_value(ret, 6);  // untruncated length should be returned!

    ret = snprintf(buf, 0, "xxx");   // n = 0 -> no change of string
    assert_same_string(buf, "pri");  // previous result still valid!
    assert_same_value(ret, 3);       // untruncated length should be returned!

    // counting newlines
    ret = snprintf(buf, MAX_STRING, "\n\n\n");
    assert_same_string(buf, "\n\n\n");
    assert_same_value(ret, 3);

    ret = snprintf(buf, MAX_STRING, "String %s and number %d\nsecond line\n",
                   "foo", 42);
    assert_same_string(buf, "String foo and number 42\nsecond line\n");
    assert_same_value(ret, 37);

    ret = snprintf(buf, MAX_STRING, "%zd", (size_t)(-1));
    assert_same_string(buf, "-1");
    assert_same_value(ret, 2);

    ret = snprintf(buf, MAX_STRING, "%d", -42);
    assert_same_string(buf, "-42");
    assert_same_value(ret, 3);

    ret = snprintf(buf, MAX_STRING, "%d", 0);
    assert_same_string(buf, "0");
    assert_same_value(ret, 1);

    ret = snprintf(buf, MAX_STRING, "%d", 42);
    assert_same_string(buf, "42");
    assert_same_value(ret, 2);

    ret = snprintf(buf, MAX_STRING, "%x", 0x12ab);
    assert_same_string(buf, "12ab");
    assert_same_value(ret, 4);

    ret = snprintf(buf, MAX_STRING, "%zX", (size_t)(0xABCDEF));
    assert_same_string(buf, "ABCDEF");
    assert_same_value(ret, 6);

    ret = snprintf(buf, MAX_STRING, "%zX", (size_t)(0xFFFFFFFF));
    assert_same_string(buf, "FFFFFFFF");
    assert_same_value(ret, 8);

    ret = snprintf(buf, MAX_STRING, "%zx", (size_t)(256));
    assert_same_string(buf, "100");
    assert_same_value(ret, 3);

    ret = snprintf(buf, MAX_STRING, "%03d", 7);
    assert_same_string(buf, "007");
    assert_same_value(ret, 3);

    ret = snprintf(buf, MAX_STRING, "%03d", -7);
    assert_same_string(buf, "-07");
    assert_same_value(ret, 3);

    ret = snprintf(buf, MAX_STRING, "%08zd", (ssize_t)5678);
    assert_same_string(buf, "00005678");
    assert_same_value(ret, 8);

    ret = snprintf(buf, MAX_STRING, "%08zd", (ssize_t)-5678);
    assert_same_string(buf, "-0005678");
    assert_same_value(ret, 8);

    ret = snprintf(buf, MAX_STRING, "%8zd", (ssize_t)5678);
    assert_same_string(buf, "    5678");
    assert_same_value(ret, 8);

    ret = snprintf(buf, MAX_STRING, "%8zd", (ssize_t)-5678);
    assert_same_string(buf, "   -5678");
    assert_same_value(ret, 8);

    ret = snprintf(buf, MAX_STRING, "0x%08zx", (ssize_t)0xdeadf00d);
    assert_same_string(buf, "0xdeadf00d");
    assert_same_value(ret, 10);

    ret = snprintf(buf, MAX_STRING, "0x%08zX", (ssize_t)0xdeadf00d);
    assert_same_string(buf, "0xDEADF00D");
    assert_same_value(ret, 10);

    if (sizeof(size_t) == 8)
    {
        ret = snprintf(buf, MAX_STRING, "%zu", (size_t)(-1));
        assert_same_string(buf, "18446744073709551615");
        assert_same_value(ret, 20);

        ret = snprintf(buf, MAX_STRING, "%zx", (size_t)(-1));
        assert_same_string(buf, "ffffffffffffffff");
        assert_same_value(ret, 16);

        ret = snprintf(buf, MAX_STRING, "0x%010zx", (ssize_t)0xdeadf00d);
        assert_same_string(buf, "0x00deadf00d");
        assert_same_value(ret, 12);

        ret = snprintf(buf, MAX_STRING, "0x%010zX", (ssize_t)0xdeadf00d);
        assert_same_string(buf, "0x00DEADF00D");
        assert_same_value(ret, 12);

        ret = snprintf(buf, MAX_STRING, "%zX", (size_t)(0xFFFFFFFFFFFF));
        assert_same_string(buf, "FFFFFFFFFFFF");
        assert_same_value(ret, 12);
    }
    else
    {
        ret = snprintf(buf, MAX_STRING, "%zu", (size_t)(-1));
        assert_same_string(buf, "4294967295");
        assert_same_value(ret, 10);

        ret = snprintf(buf, MAX_STRING, "%zx", (size_t)(-1));
        assert_same_string(buf, "ffffffff");
        assert_same_value(ret, 8);
    }

#undef MAX_STRING
}

void getc_test(char *s)
{
    const char str[] = "abcd";
    const char *file_name = "getc_test";
    size_t str_len = sizeof(str) - 1;  // excluding 0-terminator

    // write file:
    int fd = open(file_name, O_CREAT | O_WRONLY | O_TRUNC, 0755);
    assert_open_ok_fd(s, fd, file_name);
    assert_same_value(write(fd, str, str_len), str_len);
    assert_same_value(close(fd), 0);

    // open and read via getc():
    FILE *f = fopen(file_name, "r");
    assert_open_ok(s, f, file_name);

    for (size_t i = 0; i < str_len; ++i)
    {
        assert_same_value((char)getc(f), str[i]);
    }
    int c = getc(f);
    assert_same_value(c, EOF);
    assert_same_value(fclose(f), 0);

    // open and read via fgetc():
    f = fopen(file_name, "r");
    assert_open_ok(s, f, file_name);
    for (size_t i = 0; i < str_len; ++i)
    {
        assert_same_value((char)getc(f), str[i]);
    }
    c = fgetc(f);
    assert_same_value(c, EOF);
    assert_same_value(fclose(f), 0);

    // again with ungetc():
    f = fopen(file_name, "r");
    assert_open_ok(s, f, file_name);
    for (size_t i = 0; i < str_len; ++i)
    {
        int c = getc(f);
        assert_same_value((char)c, str[i]);
        ungetc(c, f);
        c = getc(f);
        assert_same_value((char)c, str[i]);
    }
    c = getc(f);
    assert_same_value(c, EOF);
    assert_same_value(fclose(f), 0);
}

void realloc_test(char *s)
{
    size_t size = 16;
    int *ptr1 = (int *)realloc(NULL, sizeof(int) * size);
    assert_no_ptr_error(ptr1);

    for (size_t i = 0; i < size; ++i)
    {
        ptr1[i] = i;
    }

    int *ptr2 = (int *)realloc(ptr1, sizeof(int) * size * 2);
    assert_no_ptr_error(ptr2);

    for (size_t i = 0; i < size; ++i)
    {
        assert_same_value(ptr2[i], i);
    }

    size = size / 2;
    int *ptr3 = (int *)realloc(ptr2, sizeof(int) * size);
    assert_no_ptr_error(ptr3);

    for (size_t i = 0; i < size; ++i)
    {
        assert_same_value(ptr3[i], i);
    }

    free(ptr3);
}

void str_test(char *s)
{
    char *haystack = "Foobar test string test";
    char *pos = strstr(haystack, "Foo");
    assert_same_value(pos, haystack);

    pos = strstr(haystack, "test");
    assert_same_value(pos, haystack + 7);

    pos = strstr(haystack, "404");
    assert_same_value(pos, NULL);

    pos = strstr(haystack, "testing");
    assert_same_value(pos, NULL);
}

void getline_test(char *s)
{
    const char *file_name = "getline_test";
    unlink(file_name);

    {
        FILE *f = fopen(file_name, "w");
        assert_open_ok(s, f, file_name);
        char *line = NULL;
        size_t line_buf_size;
        ssize_t ret = getline(&line, &line_buf_size, f);
        assert_same_value(ret, EOF);

        free(line);
        fclose(f);
    }
    {
        const char str[] = "abcd";
        size_t str_len = sizeof(str) - 1;  // excluding 0-terminator

        // write file:
        int fd = open(file_name, O_CREAT | O_WRONLY | O_TRUNC, 0755);
        assert_open_ok_fd(s, fd, file_name);
        assert_same_value(write(fd, str, str_len), str_len);
        assert_same_value(close(fd), 0);

        FILE *f = fopen(file_name, "r");
        assert_open_ok(s, f, file_name);
        char *line = NULL;
        size_t line_buf_size;
        ssize_t ret = getline(&line, &line_buf_size, f);
        assert_same_value(ret, str_len);
        assert_same_string(line, str);

        ret = getline(&line, &line_buf_size, f);
        assert_same_value(ret, EOF);

        free(line);
        fclose(f);
    }
    {
        const char str[] = "abcdXY\n\nefg\n";
        size_t str_len = sizeof(str) - 1;  // excluding 0-terminator

        // write file:
        int fd = open(file_name, O_CREAT | O_WRONLY | O_TRUNC, 0755);
        assert_open_ok_fd(s, fd, file_name);
        assert_same_value(write(fd, str, str_len), str_len);
        assert_same_value(close(fd), 0);

        FILE *f = fopen(file_name, "r");
        assert_open_ok(s, f, file_name);
        char *line = malloc(4);  // intentionally too small
        if (line == NULL)
        {
            printf("%s: ERROR, tiny malloc failed (errno: %s)\n", s,
                   strerror(errno));
        }
        size_t line_buf_size = 4;
        ssize_t ret = getline(&line, &line_buf_size, f);
        if (line_buf_size < 7) exit(-1);
        assert_same_value(ret, 7);  // incl new line
        assert_same_string(line, "abcdXY\n");

        ret = getline(&line, &line_buf_size, f);
        assert_same_value(ret, 1);
        assert_same_string(line, "\n");

        ret = getline(&line, &line_buf_size, f);
        assert_same_value(ret, 4);
        assert_same_string(line, "efg\n");

        ret = getline(&line, &line_buf_size, f);
        assert_same_value(ret, EOF);

        free(line);
        fclose(f);
    }
}

void strtoul_test(char *s)
{
    char *end = NULL;
    unsigned long n;

    const char *test_str1 = "   42foo";
    n = strtoul(test_str1, &end, 10);
    assert_same_value(n, 42);
    assert_same_value(*end, 'f');

    const char *test_str2 = "0123";
    n = strtoul(test_str2, &end, 10);
    assert_same_value(n, 123);
    assert_same_value(*end, 0);

    const char *test_str3 = " -123456a";
    n = strtoul(test_str3, &end, 10);
    assert_same_value(n, -123456);
    assert_same_value(*end, 'a');
}

struct test quicktests_common[] = {
    {dev_null, "dev_null"},
    {dev_zero, "dev_zero"},
    {lseek_test, "lseek"},
    {ctype_test, "ctype"},
    {printf_test, "printf"},
    {getc_test, "getc"},
    {realloc_test, "realloc"},
    {str_test, "str"},
    {getline_test, "getline"},
    {strtoul_test, "strtoul"},

    {0, 0},
};

struct test slowtests_common[] = {
    {0, 0},
};

// helper functions

void assert_open_ok(const char *test_name, FILE *f, const char *file_name)
{
    if (f == NULL)
    {
        printf("%s: error: could not open %s (errno: %s)!\n", test_name,
               file_name, strerror(errno));
        exit(1);
    }
}

void assert_open_ok_fd(const char *test_name, int fd, const char *file_name)
{
    if (fd < 0)
    {
        printf("%s: error: could not open %s (errno: %s)!\n", test_name,
               file_name, strerror(errno));
        exit(1);
    }
}

void assert_null_s(const char *test_name, ssize_t value)
{
    if (value != 0)
    {
        printf("%s: error: expected return value 0\n", test_name);
        exit(1);
    }
}

void assert_write_to_file(const char *test_name, int fd, const char *string)
{
    size_t str_len = strlen(string);

    if (write(fd, string, str_len) != str_len)
    {
        printf("%s: error: write of %zd bytes to file failed (errno: %s)\n",
               test_name, str_len, strerror(errno));
        exit(1);
    }
}
