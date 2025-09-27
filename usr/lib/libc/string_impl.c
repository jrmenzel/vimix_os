/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// avoid dublicated code, re-use kernels libs.
#include "../../kernel/lib/string.c"

#define MAX_ERROR_LENGTH 64
char g_strerror_buffer[MAX_ERROR_LENGTH];

#define CODE_STRING(C, STR) \
    case C: return STR; break;

char *strerror(int errnum)
{
    switch (errnum)
    {
        CODE_STRING(EPERM, "Operation not permitted");
        CODE_STRING(ENOENT, "No such file or directory");
        CODE_STRING(ESRCH, "No such process");
        CODE_STRING(E2BIG, "Argument list too long");
        CODE_STRING(ENOEXEC, "Exec format error");
        CODE_STRING(EBADF, "Bad file descriptor");
        CODE_STRING(ECHILD, "No child processes");
        CODE_STRING(ENOMEM, "OS is out of memory");
        CODE_STRING(EACCES, "Permission denied");
        CODE_STRING(EFAULT, "Address fault");
        CODE_STRING(ENOTBLK, "Block device required");
        CODE_STRING(ENODEV, "No such device");
        CODE_STRING(ENOTDIR, "Not a directory");
        CODE_STRING(EISDIR, "Is a directory");
        CODE_STRING(EINVAL, "Invalid argument");
        CODE_STRING(EMFILE, "Too many open files for this process");
        CODE_STRING(ENOTTY, "Not a TTY device file");
        CODE_STRING(ESPIPE, "Illegal seek, fd is a pipe");
        CODE_STRING(ENOTEMPTY, "Dir not empty");
        CODE_STRING(EOTHER, "Other error");
        CODE_STRING(EINVALSCALL, "Invalid syscall number");

        default: break;
    }

    return "Unknown error code";
}

char *strdup(const char *s)
{
    if (s == NULL) return NULL;

    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (copy == NULL) return NULL;

    strcpy(copy, s);
    return copy;
}
