/*
 * jautolock: fire up programs in case of user inactivity under X
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
#include <X11/extensions/scrnsaver.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
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

struct Action {
    struct timespec time;
    const char *command;
    pid_t pid;
};

static int setup_signalfd();
static int get_idle_time(struct timespec *idle);
static int execute_action(struct Action *action);
static int timespec_cmp(struct timespec lhs, struct timespec rhs);
static struct timespec timespec_add(struct timespec lhs, struct timespec rhs);
static struct timespec timespec_sub(struct timespec lhs, struct timespec rhs);
static void timespec_minify(struct timespec *lhs, struct timespec rhs);
static void timespec_maxify(struct timespec *lhs, struct timespec rhs);
static void timespec_minify_pos(struct timespec *lhs, struct timespec a,
        struct timespec b);

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

    const struct timespec actdiff_threshold = {0, 10000000}; // 10ms
    const struct timespec min_sleep_time    = {0, 10000000}; // 10ms
    struct timespec act;    // last user activity time
    struct timespec last;   // last time checked
    struct timespec offset = {};     // TODO what is this
    struct timespec nextoffset = {}; // TODO what is this

    if(clock_gettime(CLOCK_MONOTONIC, &last) < 0) {
        syslog(LOG_ERR, "clock_gettime: %s", strerror(errno));
        return 1;
    }
    act = last;

    while(true) {
        struct timespec timeout;
        timeout.tv_sec = 365 * 86400; // almost infinity
        timeout.tv_nsec = 0;
        for(unsigned i = 0; i < n_action; i++) {
            struct timespec t1 =
                    timespec_sub(timespec_add(act, actions[i].time), offset);
            timespec_minify_pos(&timeout, t1, last);

            timespec_minify_pos(&timeout, actions[i].time, nextoffset);
        }
        timespec_maxify(&timeout, min_sleep_time);

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sigfd, &readfds);

        if(pselect(sigfd + 1, &readfds, NULL, NULL, &timeout, NULL) < 0) {
            syslog(LOG_ERR, "pselect: %s", strerror(errno));
            return 1;
        }
        if(FD_ISSET(sigfd, &readfds)) {
            struct signalfd_siginfo siginfo;
            if(read(sigfd, &siginfo, sizeof(siginfo)) < 0) {
                syslog(LOG_ERR, "read: %s", strerror(errno));
                goto cancel_readfd;
            }
            if(siginfo.ssi_signo != SIGCHLD) {
                syslog(LOG_WARNING, "unknown signum: %d",
                        (int) siginfo.ssi_signo);
                goto cancel_readfd;
            }
            pid_t pid = wait(NULL);
            if(pid < 0) {
                syslog(LOG_ERR, "wait: %s", strerror(errno));
                goto cancel_readfd;
            }
            nextoffset.tv_sec = nextoffset.tv_nsec = 0;
            for(unsigned i = 0; i < n_action; i++) {
                if(actions[i].pid == pid)
                    actions[i].pid = 0;
                if(actions[i].pid)
                    timespec_maxify(&nextoffset, actions[i].time);
            }
cancel_readfd:
            ;
        }

        struct timespec cur;
        if(clock_gettime(CLOCK_MONOTONIC, &cur) < 0) {
            syslog(LOG_ERR, "clock_gettime: %s", strerror(errno));
            return 1;
        }
        struct timespec idle;
        if(get_idle_time(&idle) < 0)
            return 1;
        struct timespec nextact = timespec_sub(cur, idle);
        if(timespec_cmp(timespec_sub(nextact, act), actdiff_threshold) > 0) {
            // new user activity
            act = nextact;
            offset = nextoffset;
            last = act;
        }
        // check range (last - act + offset, cur - act + offset]
        struct timespec tbegin = timespec_add(timespec_sub(last, act), offset);
        struct timespec tend   = timespec_add(timespec_sub(cur,  act), offset);
#if 0
        fprintf(stderr, "last=%ld.%09ld cur=%ld.%09ld act=%ld.%09ld offset=%ld.%09ld "
                        "(%ld.%09ld, %ld.%09ld]\n",
                last.tv_sec, last.tv_nsec,
                cur.tv_sec, cur.tv_nsec,
                act.tv_sec, act.tv_nsec,
                offset.tv_sec, offset.tv_nsec,
                tbegin.tv_sec, tbegin.tv_nsec,
                tend.tv_sec, tend.tv_nsec);
#endif
        for(unsigned i = 0; i < n_action; i++) {
            if(timespec_cmp(tbegin, actions[i].time) < 0 &&
                    timespec_cmp(actions[i].time, tend) <= 0) {
                execute_action(actions + i);
                timespec_maxify(&nextoffset, actions[i].time);
            }
        }
        last = cur;
    }
}

static int setup_signalfd() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    return signalfd(-1, &mask, SFD_CLOEXEC);
}

static int get_idle_time(struct timespec *idle) {
    Display *display = XOpenDisplay(NULL);
    if(!display) {
        syslog(LOG_ERR, "XOpenDisplay failed\n");
        return -1;
    }
    XScreenSaverInfo *info = XScreenSaverAllocInfo();
    if(!info) {
        syslog(LOG_ERR, "XScreenSaverAllocInfo failed\n");
        XCloseDisplay(display);
        return -1;
    }
    if(!XScreenSaverQueryInfo(display,
            XDefaultRootWindow(display), info)) {
        syslog(LOG_ERR, "XScreenSaverQueryInfo failed\n");
        XFree(info);
        XCloseDisplay(display);
        return -1;
    }

    idle->tv_sec  = info->idle / 1000;
    idle->tv_nsec = info->idle % 1000 * 1000000;

    XFree(info);
    XCloseDisplay(display);
    return 0;
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

static int timespec_cmp(struct timespec lhs, struct timespec rhs) {
    if(lhs.tv_sec < rhs.tv_sec)
        return -1;
    if(lhs.tv_sec > rhs.tv_sec)
        return 1;
    if(lhs.tv_nsec < rhs.tv_nsec)
        return -1;
    if(lhs.tv_nsec > rhs.tv_nsec)
        return 1;
    return 0;
}
static struct timespec timespec_add(struct timespec lhs, struct timespec rhs) {
    lhs.tv_sec += rhs.tv_sec;
    lhs.tv_nsec += rhs.tv_nsec;
    if(lhs.tv_nsec >= 1000000000) {
        lhs.tv_nsec -= 1000000000;
        lhs.tv_sec += 1;
    }
    return lhs;
}
static struct timespec timespec_sub(struct timespec lhs, struct timespec rhs) {
    lhs.tv_sec -= rhs.tv_sec;
    if(lhs.tv_nsec < rhs.tv_nsec) {
        lhs.tv_nsec += 1000000000 - rhs.tv_nsec;
        lhs.tv_sec -= 1;
    } else {
        lhs.tv_nsec -= rhs.tv_nsec;
    }
    return lhs;
}
static void timespec_minify(struct timespec *lhs, struct timespec rhs) {
    if(timespec_cmp(*lhs, rhs) > 0)
        *lhs = rhs;
}
static void timespec_maxify(struct timespec *lhs, struct timespec rhs) {
    if(timespec_cmp(*lhs, rhs) < 0)
        *lhs = rhs;
}
static void timespec_minify_pos(struct timespec *lhs, struct timespec a,
        struct timespec b) {
    if(timespec_cmp(a, b) > 0)
        timespec_minify(lhs, timespec_sub(a, b));
}
