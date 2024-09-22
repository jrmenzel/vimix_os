/* SPDX-License-Identifier: MIT */

#include "usertests.h"

#if defined(BUILD_ON_HOST)
// we are not the only process on the host, so comparing memory isn't useful
int countfree() { return 0; }

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
    assert_open_ok(s, fd, "/dev/null");

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
    assert_open_ok(s, fd, "/dev/zero");

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
    assert_open_ok(s, fd, file_name);

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

struct test quicktests_common[] = {
    {dev_null, "dev_null"},
    {dev_zero, "dev_zero"},
    {lseek_test, "lseek_test"},

    {0, 0},
};

struct test slowtests_common[] = {
    {0, 0},
};

// helper functions

void assert_open_ok(const char *test_name, int fd, const char *file_name)
{
    if (fd < 0)
    {
        printf("%s: error: could not open %s!\n", test_name, file_name);
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
        printf("%s: error: write of %zd bytes to file failed\n", test_name,
               str_len);
        exit(1);
    }
}
