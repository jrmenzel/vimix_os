/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Functions return a pointer to this global struct which makes them not
// thread-safe.
#define MAX_LINE_LEN 128
struct get_shadow_buffer
{
    char line[MAX_LINE_LEN];  // ret_group line buffer points into this
    struct spwd ret_shadow;
} g_shadow = {0};

struct spwd *getspnam(const char *name)
{
    FILE *f = fopen("/etc/shadow", "r");
    if (!f)
    {
        return NULL;
    }

    while (fgets(g_shadow.line, sizeof(g_shadow.line), f))
    {
        char *saveptr;
        char *spw_sp_namp = strtok_r(g_shadow.line, ":\n", &saveptr);
        char *spw_sp_pwdp = strtok_r(NULL, ":\n", &saveptr);

        if (!spw_sp_namp || !spw_sp_pwdp)
        {
            continue;  // malformed line
        }

        if (strcmp(spw_sp_namp, name) == 0)
        {
            g_shadow.ret_shadow.sp_namp = spw_sp_namp;
            g_shadow.ret_shadow.sp_pwdp = spw_sp_pwdp;

            // The rest of the fields are not used in VIMIX, set to zero
            g_shadow.ret_shadow.sp_lstchg = 0;
            g_shadow.ret_shadow.sp_min = 0;
            g_shadow.ret_shadow.sp_max = 0;
            g_shadow.ret_shadow.sp_warn = 0;
            g_shadow.ret_shadow.sp_inact = 0;
            g_shadow.ret_shadow.sp_expire = 0;
            g_shadow.ret_shadow.sp_flag = 0;

            fclose(f);
            return &g_shadow.ret_shadow;
        }
    }

    fclose(f);
    errno = ENOENT;
    return NULL;
}