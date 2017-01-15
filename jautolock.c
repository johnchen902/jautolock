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
#include "die.h"
#include "fifo.h"
#include "timespecop.h"
#include "timecalc.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int mask_and_signalfd(int signum);
static void prepare_cleanup_fifo();
static void execute_action(struct Action *action);
static pid_t get_dead_child_pid(int sigfd);

struct Action test_actions[2] = {
    {{50, 0}, "notify-send jautolock \"10 seconds before locking\"", 0},
    {{60, 0}, "i3lock -nc 000000", 0}
};

int main(int argc, char **argv) {
    if(argc > 2)
        die("usage: %s [message]\n", argv[0]);
    if(argc == 2) {
        int fd = open_fifo_write();
        if(write(fd, argv[1], strlen(argv[1]) + 1) < 0)
            die("write() failed. Reason: %s\n", strerror(errno));
        close(fd);
        return 0;
    }

    int sigfd = mask_and_signalfd(SIGCHLD);
    int fifofd = open_fifo_read();
    prepare_cleanup_fifo();

    unsigned n_action = sizeof(test_actions) / sizeof(test_actions[0]);
    struct Action *actions = test_actions;

    timecalc_reset();

    while(true) {
        struct timespec timeout;
        timecalc_sleep(&timeout, actions, n_action);

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sigfd, &readfds);
        FD_SET(fifofd, &readfds);
        int maxfd = sigfd;
        if(maxfd < fifofd)
            maxfd = fifofd;

        if(pselect(maxfd + 1, &readfds, NULL, NULL, &timeout, NULL) < 0)
            die("pselect() failed. Reason: %s\n", strerror(errno));

        if(FD_ISSET(sigfd, &readfds)) {
            int pid = get_dead_child_pid(sigfd);
            for(unsigned i = 0; i < n_action; i++)
                if(actions[i].pid == pid)
                    actions[i].pid = 0;
            timecalc_next_offset(actions, n_action);
        }
        if(FD_ISSET(fifofd, &readfds)) {
            char buf[1024];
            ssize_t sz = read(fifofd, buf, sizeof(buf) - 1);
            if(sz < 0)
                die("read() failed. Reason: %s\n", strerror(errno));

            buf[sz] = '\0';
            if(strcmp(buf, "exit") == 0)
                break;
            if(strncmp(buf, "firenow ", 8) == 0 && buf[8] != '\0') {
                char *endptr;
                long x = strtol(buf + 8, &endptr, 0);
                if(*endptr == '\0' && x >= 0 && x < n_action &&
                        actions[x].pid == 0) {
                    execute_action(actions + x);
                    timecalc_firenow(actions[x].time);
                }
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

static int mask_and_signalfd(int signum) {
    sigset_t mask;
    if(sigemptyset(&mask) < 0)
        die("sigemptyset() failed. Reason: %s\n", strerror(errno));
    if(sigaddset(&mask, signum) < 0)
        die("sigaddset() failed. Reason: %s\n", strerror(errno));
    if(sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
        die("sigprocmask() failed. Reason: %s\n", strerror(errno));
    int fd = signalfd(-1, &mask, SFD_CLOEXEC);
    if(fd < 0)
        die("signalfd() failed. Reason: %s\n", strerror(errno));
    return fd;
}

static void unlink_fifo_atexit_wrapper() {
    unlink_fifo();
}
static void unlink_fifo_signal_wrapper(int sig) {
    unlink_fifo();
    signal(sig, SIG_DFL);
    raise(sig);
}
static void prepare_cleanup_fifo() {
    atexit(unlink_fifo_atexit_wrapper);
    struct sigaction act = {0};
    act.sa_handler = unlink_fifo_signal_wrapper;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}

static pid_t get_dead_child_pid(int sigfd) {
    struct signalfd_siginfo siginfo;
    if(read(sigfd, &siginfo, sizeof(siginfo)) < 0)
        die("read() failed. Reason: %s\n", strerror(errno));
    if(siginfo.ssi_signo != SIGCHLD)
        die("SIGCHLD expected, not %s\n", strsignal(siginfo.ssi_signo));
    pid_t pid = wait(NULL);
    if(pid < 0)
        die("wait() failed. Reason: %s\n", strerror(errno));
    return pid;
}

static void execute_action(struct Action *action) {
    if(action->pid != 0) {
        fprintf(stderr, "WARNING: attempted to fire a running action");
        return;
    }

    int pid = fork();
    if(pid == 0) {
        execlp("sh", "sh", "-c", action->command, NULL);
        _exit(EXIT_FAILURE); // just in case
    }
    if(pid < 0)
        die("fork() failed. Reason: %s\n", strerror(errno));
    action->pid = pid;
}

