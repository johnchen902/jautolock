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
#include "die.h"
#include "messages.h"
#include "tasks.h"
#include "timecalc.h"
#include "userconfig.h"

static char *concat_strings(char **list, int n);
static int mask_and_signalfd(int signum);
static void signal_handler(int sig);
static pid_t get_dead_child_pid(int sigfd);

static struct option long_options[] = {
    {"config", required_argument, 0, 'c'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

static sig_atomic_t exit_on_signal = 0;

int main(int argc, char **argv) {
    char *config_file = NULL;
    while(true) {
        int option_index = 0;
        int opt = getopt_long(argc, argv, "c:h", long_options, &option_index);
        if(opt == -1)
            break;
        switch(opt) {
        case 'c':
            if(config_file)
                die("multiple configuration file specified");
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
    read_config(config_file);
    free(config_file);

    if(optind < argc) {
        char *s = concat_strings(argv + optind, argc - optind);
        // TODO implement
        if(argc - optind >= 2)
            free(s);
        free_config();
        return 0;
    }

    struct Task *tasks;
    unsigned n_task = get_tasks(&tasks);
    if(n_task == 0)
        die("Error: No task specifed in configuration.\n");

    int sigfd = mask_and_signalfd(SIGCHLD);

    {
        struct sigaction act = {0};
        act.sa_handler = signal_handler;
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGTERM, &act, NULL);
    }

    timecalc_init();

    while(!exit_on_signal) {
        struct timespec timeout;
        timecalc_cycle(&timeout, tasks, n_task);

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sigfd, &readfds);
        int maxfd = sigfd;

        if(pselect(maxfd + 1, &readfds, NULL, NULL, &timeout, NULL) < 0) {
            if(errno == EINTR && exit_on_signal)
                break;
            die("pselect() failed. Reason: %s\n", strerror(errno));
        }

        if(FD_ISSET(sigfd, &readfds)) {
            int pid = get_dead_child_pid(sigfd);
            for(unsigned i = 0; i < n_task; i++)
                if(tasks[i].pid == pid)
                    tasks[i].pid = 0;
        }
    }

    free(tasks);
    free_config();

    if(exit_on_signal > 0) {
        int sig = exit_on_signal;
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

/**
 * Mask the specified signal and open a file
 * descripter to receive the signal.
 *
 * Return the file descripter.
 */
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

/**
 * Just the signal handler.
 */
static void signal_handler(int sig) {
    exit_on_signal = sig;
}

/**
 * Read a signal from the specified file descripter.
 * The signal read must be SIGCHLD.
 *
 * Then, wait() for a dead child.
 * Return the pid of the child.
 */
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
