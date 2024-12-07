/* SPDX-License-Identifier: MIT */

#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(BUILD_ON_HOST)
#include <linux/limits.h>
#else
#include <kernel/limits.h>
#endif

struct Parameters
{
    bool print_directory_name;
    bool print_dot_dotdot;
    bool print_hidden;
};

void set_default_parameters(struct Parameters *parameters)
{
    parameters->print_directory_name = false;
    parameters->print_dot_dotdot = false;
    parameters->print_hidden = true;
}

const int S_OK = 0;
const int S_MINOR_ERROR = 1;
const int S_SERIOUS_ERROR = 2;

int max_error(int a, int b) { return (a > b) ? a : b; }

const char *fmtname(const char *path_name)
{
    static char buf[NAME_MAX + 1];

    // Find first character after last slash.
    const char *p = path_name + strlen(path_name);
    while (p >= path_name && *p != '/')
    {
        p--;
    }
    p++;

    size_t name_length = strlen(p);
    if (name_length >= NAME_MAX)
    {
        return p;
    }

    // Return blank-padded name.
    memmove(buf, p, name_length);
    memset(buf + name_length, ' ', NAME_MAX - name_length);
    buf[name_length] = 0;

    return buf;
}

char type_to_char(mode_t mode)
{
    if (S_ISBLK(mode)) return 'b';
    if (S_ISCHR(mode)) return 'c';
    if (S_ISDIR(mode)) return 'd';
    if (S_ISREG(mode)) return '.';
    if (S_ISFIFO(mode)) return 'p';
    return ' ';
}

void print_access(mode_t mode)
{
    char str[11] = ".rwxrwxrwx";
    str[0] = type_to_char(mode);

    size_t pos = 9;
    for (size_t i = 0; i < 9; ++i)
    {
        mode_t bit = mode & 0x01;
        mode = mode >> 1;

        if (!bit) str[pos] = '-';
        pos--;
    }

    printf("%s",str);
}

void print_padded(size_t value, size_t min_chars_wide, bool min_one_space)
{
    size_t value_width = 1;
    size_t tmp = value;
    while ((tmp /= 10) > 0)
    {
        value_width++;
    }

    ssize_t missing_chars = min_chars_wide - value_width;
    if (min_one_space && missing_chars < 1)
    {
        missing_chars = 1;
    }
    if (missing_chars > 0)
    {
        for (size_t i = 0; i < missing_chars; ++i)
        {
            printf(" ");
        }
    }

    printf("%zd", value);
}

int print_file(const char *file_name, const char *full_path,
               struct Parameters *parameters)
{
    struct stat st;
    if (stat(full_path, &st) < 0) return S_SERIOUS_ERROR;

    // print_padded(st.st_ino, 4, false);
    // printf(" ");
    print_access(st.st_mode);

    // print_padded(st.st_uid, 3, true);
    // print_padded(st.st_gid, 3, true);

    print_padded(st.st_size, 8, true);
    printf(" B ");

    printf("%s\n", file_name);

    return 0;
}

int build_full_path(char *dst, const char *path, const char *file)
{
    strncpy(dst, path, PATH_MAX);
    size_t len = strlen(dst);
    if (dst[len - 1] != '/')
    {
        dst[len] = '/';
        len++;
    }
    size_t name_len = strlen(file);
    if (len + name_len > PATH_MAX - 1)
    {
        return S_SERIOUS_ERROR;
    }
    strncpy(dst + len, file, PATH_MAX - len);

    return S_OK;
}

enum FileVisibility
{
    DOT_OR_DOTDOT,
    HIDDEN,
    VISIBLE
};
enum FileVisibility get_visibility(const char *file_name)
{
    if (file_name[0] == '.')
    {
        if (file_name[1] == 0) return DOT_OR_DOTDOT;
        if (file_name[1] == '.' && file_name[2] == 0) return DOT_OR_DOTDOT;

        return HIDDEN;
    }

    return VISIBLE;
}

int print_dir(const char *path_name, struct Parameters *parameters)
{
    if (parameters->print_directory_name)
    {
        printf("%s:\n", path_name);
    }

    DIR *dir = opendir(path_name);
    if (dir == NULL)
    {
        return S_SERIOUS_ERROR;
    }

    char full_path[PATH_MAX];
    struct dirent *dir_entry = NULL;
    while ((dir_entry = readdir(dir)))
    {
        // for all entries:
        enum FileVisibility visibility = get_visibility(dir_entry->d_name);
        if (visibility == HIDDEN && parameters->print_hidden == false)
        {
            continue;
        }
        if (visibility == DOT_OR_DOTDOT &&
            parameters->print_dot_dotdot == false)
        {
            continue;
        }

        build_full_path(full_path, path_name, dir_entry->d_name);

        print_file(dir_entry->d_name, full_path, parameters);
    }

    closedir(dir);

    return S_OK;
}

int ls(const char *path_name, struct Parameters *parameters)
{
    struct stat st;
    if (stat(path_name, &st) < 0) return S_SERIOUS_ERROR;

    int return_code = S_OK;
    if (S_ISDIR(st.st_mode))
    {
        return_code = print_dir(path_name, parameters);
    }
    else
    {
        return_code = print_file(fmtname(path_name), path_name, parameters);
    }

    return return_code;
}

int main(int argc, char *argv[])
{
    struct Parameters parameters;
    set_default_parameters(&parameters);

    if (argc < 2)
    {
        return ls(".", &parameters);
    }

    // print multiple files and/or directories:
    parameters.print_directory_name = true;
    int highest_error = S_OK;
    for (size_t i = 1; i < argc; i++)
    {
        int error = ls(argv[i], &parameters);
        highest_error = max_error(highest_error, error);
    }

    return highest_error;
}
