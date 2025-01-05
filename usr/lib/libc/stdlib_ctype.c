/* SPDX-License-Identifier: MIT */

#include <ctype.h>

int isprint(int arg) { return (arg >= 32 && arg <= 126); }

int iscntrl(int arg)
{
    if (arg >= 0 && arg <= 31) return 1;
    if (arg == 127) return 1;
    return 0;
}

int isalnum(int arg) { return isalpha(arg) || isdigit(arg); }

int isalpha(int arg) { return islower(arg) || isupper(arg); }

int isdigit(int arg) { return (arg >= '0' && arg <= '9'); }

int isgraph(int arg) { return (arg >= 33 && arg <= 126); }

int islower(int arg)
{
    if (arg >= 'a' && arg <= 'z') return 1;
    return 0;
}

int isupper(int arg)
{
    if (arg >= 'A' && arg <= 'Z') return 1;
    return 0;
}

int ispunct(int arg)
{
    if (arg >= 33 && arg <= 47) return 1;
    if (arg >= 58 && arg <= 64) return 1;
    if (arg >= 91 && arg <= 96) return 1;
    if (arg >= 123 && arg <= 126) return 1;

    return 0;
}

int isspace(int arg)
{
    // \t = 9, \n = 10, \v = 11, \f = 12, \r = 13
    return (('\t' <= arg && arg <= '\r') || (arg == ' '));
}

int isxdigit(int arg)
{
    if (isdigit(arg)) return 1;
    if (arg >= 'a' && arg <= 'f') return 1;
    if (arg >= 'A' && arg <= 'F') return 1;
    return 0;
}

int tolower(int arg)
{
    if (isupper(arg))
    {
        return arg + ('a' - 'A');
    }
    return arg;
}

int toupper(int arg)
{
    if (islower(arg))
    {
        return arg - ('a' - 'A');
    }
    return arg;
}