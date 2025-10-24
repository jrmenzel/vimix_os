/* SPDX-License-Identifier: MIT */

// required on linux for getresuid/getresgid
#define _GNU_SOURCE

#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *user_from_uid(uid_t uid)
{
    struct passwd *pw = getpwuid(uid);
    return pw ? pw->pw_name : "<unknown>";
}

static const char *group_from_gid(gid_t gid)
{
    struct group *gr = getgrgid(gid);
    return gr ? gr->gr_name : "<unknown>";
}

int main(int argc, char **argv)
{
    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;

    if (getresuid(&ruid, &euid, &suid) < 0) return 1;
    if (getresgid(&rgid, &egid, &sgid) < 0) return 1;

    gid_t groups[NGROUPS_MAX];
    size_t ngroups = getgroups(NGROUPS_MAX, groups);

    if (ngroups < 0)
    {
        ngroups = 0;
        fprintf(stderr, "getgroups failed: %s\n", strerror(errno));
    }

    printf("uid=%d(%s) gid=%d(%s) ", (int)euid, user_from_uid(euid), (int)egid,
           group_from_gid(egid));

    if (ngroups > 0)
    {
        printf("groups=");
        for (size_t i = 0; i < ngroups; i++)
        {
            if (i > 0) printf(",");
            printf("%d(%s)", (int)groups[i], group_from_gid(groups[i]));
        }
    }
    printf("\n");

    // printf("ruid=%d(%s) rgid=%d(%s)\n", (int)ruid, user_from_uid(ruid),
    //        (int)rgid, group_from_gid(rgid));
    // printf("suid=%d(%s) sgid=%d(%s)\n", (int)suid, user_from_uid(suid),
    //        (int)sgid, group_from_gid(sgid));

    return 0;
}
