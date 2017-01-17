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
#include "jautolock.h"
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "tasks.h"
#include "config.h"
#include "die.h"
#include "fifo.h"
#include "timecalc.h"

static char *concat_strings(char **list, int n);
static int mask_and_signalfd(int signum);
static void signal_handler(int sig);
static pid_t get_dead_child_pid(int sigfd);

static struct option long_options[] = {
    {"config", required_argument, 0, 'c'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

static sig_atomic_t exiting_main_loop = 0;

void exit_main_loop() {
    exiting_main_loop = -1;
}

int main(int argc, char **argv) {
    char *config_file = NULL;
    while(true) {
        int option_index = 0;
        int opt = getopt_long(argc, argv, "c:h", long_options, &option_index);
        if(opt == -1)
            break;
        switch(opt) {
        case 'c':
            config_file = strdup(optarg);
            if(!config_file)
                die("strdup() failed. Reason: %s\n", strerror(errno));
            break;
        case 'h':
            printf("jautolock © 2017 Pochang Chen\n"
                   "Usage: %s [-c <configfile>] [-h] [<message>]\n", argv[0]);
            return 0;
        }
    }
    // NOTE : read_config frees config_file
    read_config(config_file);

    if(optind < argc) {
        char *s = concat_strings(argv + optind, argc - optind);
        int fd = open_fifo_write();
        if(write(fd, s, strlen(s) + 1) < 0)
            die("write() failed. Reason: %s\n", strerror(errno));
        close(fd);
        if(argc - optind >= 2)
            free(s);
        return 0;
    }

    struct Task *tasks;
    unsigned n_task = get_tasks(&tasks);

    int sigfd = mask_and_signalfd(SIGCHLD);

    int fifofd = open_fifo_read();
    {
        struct sigaction act = {0};
        act.sa_handler = signal_handler;
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGTERM, &act, NULL);
    }

    timecalc_init();

    while(!exiting_main_loop) {
        struct timespec timeout;
        timecalc_cycle(&timeout, tasks, n_task);

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sigfd, &readfds);
        FD_SET(fifofd, &readfds);
        int maxfd = sigfd;
        if(maxfd < fifofd)
            maxfd = fifofd;

        if(pselect(maxfd + 1, &readfds, NULL, NULL, &timeout, NULL) < 0) {
            if(exiting_main_loop)
                break;
            die("pselect() failed. Reason: %s\n", strerror(errno));
        }

        if(FD_ISSET(sigfd, &readfds)) {
            int pid = get_dead_child_pid(sigfd);
            for(unsigned i = 0; i < n_task; i++)
                if(tasks[i].pid == pid)
                    tasks[i].pid = 0;
        }
        if(FD_ISSET(fifofd, &readfds)) {
            char buf[1024];
            ssize_t sz = read(fifofd, buf, sizeof(buf) - 1);
            if(sz < 0)
                die("read() failed. Reason: %s\n", strerror(errno));

            buf[sz] = '\0';
            if(strcmp(buf, "exit") == 0)
                exit_main_loop();
            if(strncmp(buf, "firenow ", 8) == 0) {
                const char *name = buf + 8;
                for(unsigned i = 0; i < n_task; i++) {
                    if(strcmp(tasks[i].name, name) == 0 &&
                            tasks[i].pid == 0) {
                        execute_task(tasks + i);
                    }
                }
            }
        }
    }

    unlink_fifo();
    free(tasks);
    free_config();

    if(exiting_main_loop > 0) {
        int sig = exiting_main_loop;
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

// concat list[0] to list[n - 1] together
static char *concat_strings(char **list, int n) {
    if(n == 1)
        return *list;
    char *s = *list;
    for(int i = 1; i < n; i++) {
        char *t;
        if(asprintf(&t, "%s %s", s, list[i]) < 0)
            die("asprintf() failed.");
        if(i > 1)
            free(s);
        s = t;
    }
    return s;
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

static void signal_handler(int sig) {
    exiting_main_loop = sig;
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
