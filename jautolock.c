/*
 * jautolock: fire up programs in case of user inactivity under X
 *
 * Copyright (C) 2017 Pochang Chen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "action.h"
#include "timespecop.h"
#include "timecalc.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

static int setup_signalfd();
static int execute_action(struct Action *action);
static pid_t get_dead_child_pid(int sigfd);

struct Action test_actions[2] = {
    {{50, 0}, "notify-send jautolock \"10 seconds before locking\"", 0},
    {{60, 0}, "i3lock -nc 000000", 0}
};

int main() {
    openlog(NULL, LOG_PERROR, LOG_USER);

    int sigfd = setup_signalfd();
    if(sigfd < 0) {
        syslog(LOG_ERR, "setup_signalfd: %s", strerror(errno));
        return 1;
    }

    unsigned n_action = sizeof(test_actions) / sizeof(test_actions[0]);
    struct Action *actions = test_actions;

    timecalc_reset();

    while(true) {
        struct timespec timeout;
        timecalc_sleep(&timeout, actions, n_action);

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sigfd, &readfds);

        if(pselect(sigfd + 1, &readfds, NULL, NULL, &timeout, NULL) < 0) {
            syslog(LOG_ERR, "pselect: %s", strerror(errno));
            return 1;
        }

        if(FD_ISSET(sigfd, &readfds)) {
            int pid = get_dead_child_pid(sigfd);
            if(pid >= 0) {
                for(unsigned i = 0; i < n_action; i++)
                    if(actions[i].pid == pid)
                        actions[i].pid = 0;
                timecalc_next_offset(actions, n_action);
            }
        }

        struct timespec tbegin, tend;
        timecalc_check_range(&tbegin, &tend);
        for(unsigned i = 0; i < n_action; i++) {
            if(timespec_between_oc(tbegin, actions[i].time, tend)) {
                execute_action(actions + i);
            }
        }
        timecalc_next_offset(actions, n_action);
    }
}

static int setup_signalfd() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    return signalfd(-1, &mask, SFD_CLOEXEC);
}

static pid_t get_dead_child_pid(int sigfd) {
    struct signalfd_siginfo siginfo;
    if(read(sigfd, &siginfo, sizeof(siginfo)) < 0) {
        syslog(LOG_ERR, "read: %s", strerror(errno));
        return -1;
    }
    if(siginfo.ssi_signo != SIGCHLD) {
        syslog(LOG_WARNING, "Received signal %d. SIGCHLD expected",
                (int) siginfo.ssi_signo);
        return -1;
    }
    pid_t pid = wait(NULL);
    if(pid < 0) {
        syslog(LOG_ERR, "wait: %s", strerror(errno));
        return -1;
    }
    return pid;
}


static int execute_action(struct Action *action) {
    if(action->pid != 0) {
        syslog(LOG_WARNING, "trying to execute running action");
        return -1;
    }

    int pid = fork();
    if(pid == 0) {
        execlp("sh", "sh", "-c", action->command, NULL);
        _exit(1); // just in case
    }
    if(pid < 0) {
        syslog(LOG_ERR, "fork: %s", strerror(errno));
        return -1;
    }
    action->pid = pid;
    return 0;
}

