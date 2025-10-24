/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Functions return a pointer to this global struct which makes them not
// thread-safe.
#define MAX_LINE_LEN 128
struct get_group_buffer
{
    char line[MAX_LINE_LEN];  // ret_group line buffer points into this
    struct group ret_group;

    char **member_list;
    size_t member_list_size;
} g_gb = {0};

FILE *g_grp_file = NULL;

bool create_group_list(struct get_group_buffer *gb, char *members_str)
{
    // count members
    size_t member_count = 1;  // number of commas + 1
    size_t members_str_len = 0;
    for (char *p = members_str; *p != 0; p++)
    {
        members_str_len++;
        if (*p == ',')
        {
            member_count++;
        }
    }
    if (gb->member_list_size < member_count + 1)
    {
        gb->member_list =
            realloc(gb->member_list, (member_count + 1) * sizeof(char *));
        if (gb->member_list == NULL)
        {
            gb->member_list_size = 0;
            errno = ENOMEM;
            return false;
        }
        gb->member_list_size = member_count + 1;
    }

    // now fill the list
    size_t member_index = 0;
    gb->member_list[member_index++] = members_str;
    for (char *p = members_str; *p != 0; p++)
    {
        if (*p == ',')
        {
            *p = '\0';
            gb->member_list[member_index++] = p + 1;
        }
    }
    gb->member_list[member_index] = NULL;  // terminate list
    return true;
}

struct group *getgroup_by_name_or_id(const char *name, uid_t gid)
{
    setgrent();
    struct group *grp = NULL;
    while ((grp = getgrent()) != NULL)
    {
        if ((grp->gr_gid == gid) || (name && (strcmp(grp->gr_name, name) == 0)))
        {
            endgrent();
            return grp;
        }
    }
    endgrent();
    errno = ENOENT;
    return NULL;
}

struct group *getgrgid(gid_t gid) { return getgroup_by_name_or_id(NULL, gid); }

struct group *getgrnam(const char *name)
{
    if (!name)
    {
        errno = EINVAL;
        return NULL;
    }

    return getgroup_by_name_or_id(name, -1);
}

int initgroups(const char *user, gid_t group)
{
    if (user == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (setgid(group) < 0) return -1;

    gid_t groups[NGROUPS_MAX];
    size_t ngroups = 0;

    setgrent();
    struct group *grp = NULL;
    while ((grp = getgrent()) != NULL)
    {
        size_t i = 0;
        while (grp->gr_mem[i])
        {
            if (strcmp(grp->gr_mem[i], user) == 0)
            {
                groups[ngroups++] = grp->gr_gid;
                break;
            }
            i++;
        }
    }
    endgrent();

    if (setgroups(ngroups, groups) < 0)
    {
        return -1;
    }

    return 0;
}

void setgrent()
{
    if (g_grp_file)
    {
        fseek(g_grp_file, 0, SEEK_SET);
    }
    else
    {
        g_grp_file = fopen("/etc/group", "r");
        if (g_grp_file == NULL)
        {
            return;  // errno is still set by fopen
        }
    }
}

void endgrent()
{
    if (g_grp_file)
    {
        fclose(g_grp_file);
        g_grp_file = NULL;
    }
}

struct group *getgrent()
{
    if (g_grp_file == NULL) setgrent();

    if (fgets(g_gb.line, MAX_LINE_LEN, g_grp_file) != NULL)
    {
        // parse line:
        // format: name:passwd:gid:member0,member1
        char *saveptr = NULL;
        char *grp_name = strtok_r(g_gb.line, ":\n", &saveptr);
        char *grp_passwd = strtok_r(NULL, ":\n", &saveptr);
        char *grp_gid_str = strtok_r(NULL, ":\n", &saveptr);
        char *grp_members = strtok_r(NULL, ":\n", &saveptr);

        if (!grp_name || !grp_passwd || !grp_gid_str || !grp_members)
        {
            return NULL;  // malformed line
        }

        g_gb.ret_group.gr_name = grp_name;
        g_gb.ret_group.gr_passwd = grp_passwd;
        g_gb.ret_group.gr_gid = (uid_t)atoi(grp_gid_str);
        g_gb.ret_group.gr_mem = g_gb.member_list;

        // can fail due to no memory. in that case errno is set.
        if (!create_group_list(&g_gb, grp_members)) return NULL;

        return &g_gb.ret_group;
    }

    return NULL;
}
