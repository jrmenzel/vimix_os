/* SPDX-License-Identifier: MIT */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <vimixutils/minmax.h>
#include <vimixutils/path.h>

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
    bool sorted;
};

void set_default_parameters(struct Parameters *parameters)
{
    parameters->print_directory_name = false;
    parameters->print_dot_dotdot = false;
    parameters->print_hidden = true;
    parameters->sorted = true;
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
    mode_t mode_copy = mode;
    for (size_t i = 0; i < 9; ++i)
    {
        mode_t bit = mode_copy & 0x01;
        mode_copy = mode_copy >> 1;

        if (!bit) str[pos] = '-';
        pos--;
    }

    if (mode & S_ISUID) str[3] = (str[3] == 'x') ? 's' : 'S';
    if (mode & S_ISGID) str[6] = (str[6] == 'x') ? 's' : 'S';
    if (mode & S_ISVTX) str[9] = (str[9] == 'x') ? 't' : 'T';

    printf("%s", str);
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

void print_user_group(uid_t uid, gid_t gid)
{
    struct passwd *pw = getpwuid(uid);
    struct group *gr = getgrgid(gid);

    if (pw)
    {
        printf(" %s", pw->pw_name);
    }
    else
    {
        printf(" %4d", (int32_t)uid);
    }
    if (gr)
    {
        printf(" %s", gr->gr_name);
    }
    else
    {
        printf(" %4d", (int32_t)gid);
    }
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

struct entry_node
{
    char name[NAME_MAX + 1];
    mode_t mode;
    uid_t uid;
    gid_t gid;
    size_t size;
    time_t mtime;
    struct entry_node *next;
};

struct entry_node *entry_node_create(struct entry_node *head, const char *name,
                                     const char *full_path)
{
    struct stat st;
    if (stat(full_path, &st) < 0)
    {
        fprintf(stderr, "stat (%s) error (errno: %s)\n", full_path,
                strerror(errno));
        return head;
    }

    struct entry_node *new_node =
        (struct entry_node *)malloc(sizeof(struct entry_node));

    strncpy(new_node->name, name, NAME_MAX);
    new_node->name[NAME_MAX] = 0;
    new_node->mode = st.st_mode;
    new_node->uid = st.st_uid;
    new_node->gid = st.st_gid;
    new_node->size = st.st_size;
    new_node->mtime = st.st_mtime;
    new_node->next = head;

    return new_node;
}

int entry_node_cmp(const void *a, const void *b)
{
    const struct entry_node *ea = *(const struct entry_node **)a;
    const struct entry_node *eb = *(const struct entry_node **)b;

    return strcmp(ea->name, eb->name);
}

void entry_node_print(struct entry_node *entry, struct Parameters *parameters)
{
    print_access(entry->mode);
    print_user_group(entry->uid, entry->gid);

    printf(" %8zd B  ", entry->size);

    time_t now = entry->mtime;
    struct tm *cal_time = localtime(&now);
    int n = printf("%d.%d.%d", cal_time->tm_mday, cal_time->tm_mon + 1,
                   1970 + cal_time->tm_year);
    for (size_t i = n; i < 11; i++) printf(" ");

    printf("%02d:%02d:%02d ", cal_time->tm_hour, cal_time->tm_min,
           cal_time->tm_sec);

    printf("%s\n", entry->name);
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

    struct entry_node *entry_list_head = NULL;
    size_t entry_count = 0;

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

        entry_list_head =
            entry_node_create(entry_list_head, dir_entry->d_name, full_path);
        entry_count++;
    }

    // now print entries:
    struct entry_node **entry_array =
        (struct entry_node **)malloc(entry_count * sizeof(struct entry_node *));
    struct entry_node *current = entry_list_head;
    for (size_t i = 0; i < entry_count; ++i)
    {
        entry_array[i] = current;
        current = current->next;
    }
    if (parameters->sorted)
    {
        qsort(entry_array, entry_count, sizeof(struct entry_node *),
              entry_node_cmp);
    }
    for (size_t i = 0; i < entry_count; ++i)
    {
        entry_node_print(entry_array[i], parameters);
        free(entry_array[i]);
    }

    closedir(dir);

    return S_OK;
}

int ls(const char *path_name, struct Parameters *parameters)
{
    struct stat st;
    if (stat(path_name, &st) < 0)
    {
        fprintf(stderr, "stat (%s) error (errno: %s)\n", path_name,
                strerror(errno));
        return S_SERIOUS_ERROR;
    }

    int return_code = S_OK;
    if (S_ISDIR(st.st_mode))
    {
        return_code = print_dir(path_name, parameters);
    }
    else
    {
        struct entry_node *entry =
            entry_node_create(NULL, fmtname(path_name), path_name);
        if (entry == NULL)
        {
            return_code = S_SERIOUS_ERROR;
        }
        else
        {
            entry_node_print(entry, parameters);
            free(entry);
            return_code = S_OK;
        }
    }

    return return_code;
}

int main(int argc, char *argv[])
{
    struct Parameters parameters;
    set_default_parameters(&parameters);
    // parameters.print_hidden = true;
    parameters.print_dot_dotdot = true;

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
        highest_error = max(highest_error, error);
    }

    return highest_error;
}
