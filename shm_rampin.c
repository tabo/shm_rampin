/*
 * shm_rampin.c - http://code.tabo.pe/shm_rampin/
 * 
 * Copyright (c) 2009 Gustavo Picon
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

void help(char *cmd_name)
{
    printf(
      "shm_rampin 1.0\n"
      "Locks/unlocks Linux shared memory segments in RAM,"
      " so they're never swapped.\n"
      "(This is linux only. In BSD, use sysctl kern.ipc.shm_use_phys)\n\n"
      "Usage: %s [OPTIONS]\n\n"
      "Options:\n"
      "  -l               locks shared memory segments\n"
      "  -u               unlocks shared memory segments\n"
      "  -i <shmid>       performs the action in a specific shmid\n"
      "                   (ipcs -m for a list)\n"
      "  -U <user_name>   performs the action in all the shmems that\n"
      "                   belong to a given user\n"
      "  -h               displays this help message\n"
      "\n\n"
      "For instance to lock all the shared memory segments that belong to\n"
      "the `postgres` user in RAM:\n\n"
      "    %s -l -U postgres\n"
      "\n"
      "And to unlock them:\n\n"
      "    %s -u -U postgres\n"
      "\n",
      cmd_name, cmd_name, cmd_name);
}

void lock_cmd(int shmid, int cmd)
{
    int result;
    struct shmid_ds sds;
    struct ipc_perm *ipcp = &sds.shm_perm;

    printf("%s shmid %d...",
           cmd==SHM_UNLOCK?"Unlocking":"Locking", shmid);
    result = shmctl(shmid, cmd, NULL);

    if (result) {
        perror("shmctl");
        exit(3);
    }

    printf(" OK! verifying... ");

    result = shmctl(shmid, IPC_STAT, &sds);

    if (result) {
        perror("shmctl");
        exit(4);
    }

    if (cmd == SHM_UNLOCK) {
        printf(ipcp->mode & SHM_LOCKED?"FAIL :(\n":"OK!\n");
    } else {
        printf(ipcp->mode & SHM_LOCKED?"OK!\n":"FAIL :(\n");
    }
}

void lock_user_shm(char *user_name, int cmd)
{
    struct shm_info si;
    struct shmid_ds sds;
    struct ipc_perm *ipcp = &sds.shm_perm;
    struct passwd *pw;
    int num = 0, shmid, maxid, id;

    pw = getpwnam(user_name);
    if (!pw) {
        fprintf(stderr, "Invalid user: '%s'\n", user_name);
        exit(5);
    }

    maxid = shmctl(0, SHM_INFO, (struct shmid_ds *)(&si));

    for (id = 0; id <= maxid; id++) {
        shmid = shmctl(id, SHM_STAT, &sds);
        if (shmid < 0 || pw->pw_uid != ipcp->uid) {
            continue;
        }
        lock_cmd(shmid, cmd);
        num++;
    }
    if (!num) {
        fprintf(stderr, "No shared memory segments under the user %s\n",
                user_name);
        exit(6);
    }
}


int main(int argc, char **argv)
{
    int c;

    char *endptr;
    int shmid = 0;
    char *user = NULL;
    int cmd = SHM_LOCK;


    while ((c = getopt(argc, argv, "hlui:U:")) != -1) {
        switch (c) {
            case 'l': /* lock */
                cmd = SHM_LOCK;
                break;
            case 'u': /* unlock */
                cmd = SHM_UNLOCK;
                break;
            case 'i': /* shmid */
                shmid = strtol(optarg, &endptr, 10);
                if (endptr == optarg || *endptr != '\0' || shmid == 0) {
                    fprintf(stderr, "Invalid shmid\n");
                    exit(2);
                }
                break;
            case 'U': /* user */
                user = optarg;
                break;
            case 'h':
                help(argv[0]);
                exit(0);
            default:
                exit(1);
        }
    }
    if (user) {
        lock_user_shm(user, cmd);
    }
    if (shmid) {
        lock_cmd(shmid, cmd);
    }
    if (!user && !shmid) {
        help(argv[0]);
    }
    return 0;
}

