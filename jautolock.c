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
#include <confuse.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "die.h"
#include "messages.h"
#include "tasks.h"
#include "timecalc.h"
#include "userconfig.h"

static char *get_socket_path(void);
static char *intersperse(char **list, int n);
static char *send_message(const char *msg, const char *socket_path);
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
    char *socket_path = NULL;
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
    cfg_t *config = read_config(config_file);
    free(config_file);

    if(!socket_path)
        socket_path = get_socket_path();

    if(optind < argc) {
        char *outmsg = intersperse(argv + optind, argc - optind);
        char *inmsg = send_message(outmsg, socket_path);
        puts(inmsg);

        free(inmsg);
        free(outmsg);
        free(socket_path);
        cfg_free(config);
        return 0;
    }

    struct Task *tasks;
    unsigned n_task = get_tasks(config, &tasks);
    if(n_task == 0)
        die("Error: No task specifed in configuration.\n");

    int sigfd = mask_and_signalfd(SIGCHLD);

    {
        struct sigaction act = {0};
        act.sa_handler = signal_handler;
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGTERM, &act, NULL);
    }

    // TODO unlink?
    int connfd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if(connfd == -1)
        die("Error: socket() failed. Reason: %s\n", strerror(errno));
    {
        struct sockaddr_un name;
        memset(&name, 0, sizeof(name));
        name.sun_family = AF_UNIX;
        strncpy(name.sun_path, socket_path, sizeof(name.sun_path) - 1);
        if(bind(connfd, (const struct sockaddr*) &name,
                   sizeof(struct sockaddr_un)) < 0)
            die("Error: bind() failed. Reason: %s\n", strerror(errno));
    }
    if(listen(connfd, 20) < 0)
        die("Error: listen() failed. Reason: %s\n", strerror(errno));

    timecalc_init();

    while(!exit_on_signal) {
        struct timespec timeout;
        timecalc_cycle(&timeout, tasks, n_task);

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sigfd, &readfds);
        FD_SET(connfd, &readfds);

        if(pselect(FD_SETSIZE, &readfds, NULL, NULL, &timeout, NULL) < 0) {
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

        if(FD_ISSET(connfd, &readfds)) {
            int datafd = accept4(connfd, NULL, NULL, SOCK_CLOEXEC);
            if(datafd == -1)
                die("accept4() failed. Reason: %s\n", strerror(errno));
            // TODO IO multiplexing
            char inmsg[1024];
            ssize_t sz = read(datafd, inmsg, sizeof(inmsg) - 1);
            if(sz < 0)
                die("read() failed. Reason: %s\n", strerror(errno));
            inmsg[sz] = '\0';
            if(strcmp(inmsg, "exit") == 0)
                exit_on_signal = -1;
            char *outmsg = handle_messages(inmsg, tasks, n_task);
            if(send(datafd, outmsg, strlen(outmsg), MSG_EOR) < 0)
                die("send() failed. Reason: %s\n", strerror(errno));
            free(outmsg);
            close(datafd);
        }
    }

    close(connfd);
    unlink(socket_path);
    free(socket_path);
    free(tasks);
    cfg_free(config);

    if(exit_on_signal > 0) {
        int sig = exit_on_signal;
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

static char *get_socket_path() {
    const char *dir = getenv("XDG_RUNTIME_DIR");
    if(!dir)
        dir = "/tmp";
    char *s;
    if(asprintf(&s, "%s/jautolock.socket", dir) < 0)
        die("asprintf() failed.");
    return s;
}

// concat list[0] to list[n - 1] together
static char *intersperse(char **list, int n) {
    if(n == 0)
        return strdup("");

    char *s = strdup(*list);
    for(int i = 1; i < n; i++) {
        char *t;
        if(asprintf(&t, "%s %s", s, list[i]) < 0)
            die("asprintf() failed.");
        free(s);
        s = t;
    }
    return s;
}

static char *send_message(const char *outmsg, const char *socket_path) {
    int datafd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if(datafd == -1)
        die("Error: socket() failed. Reason: %s\n", strerror(errno));

    struct sockaddr_un name;
    memset(&name, 0, sizeof(name));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, socket_path, sizeof(name.sun_path) - 1);
    if(connect(datafd, (const struct sockaddr*) &name,
                sizeof(struct sockaddr_un)) < 0)
        die("Error: connect() failed. Reason: %s\n", strerror(errno));

    if(send(datafd, outmsg, strlen(outmsg), MSG_EOR) < 0)
        die("send() failed. Reason: %s\n", strerror(errno));

    char buf[1024];
    ssize_t sz = read(datafd, buf, sizeof(buf) - 1);
    if(sz < 0)
        die("read() failed. Reason: %s\n", strerror(errno));
    buf[sz] = '\0';
    char *inmsg = strdup(buf);

    close(datafd);
    return inmsg;
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
