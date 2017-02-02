/*
 * timecalc.c - time related calculations for jautolock
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
#include "timecalc.h"
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "tasks.h"
#include "die.h"

static struct timespec get_idle_time(void);
static int timespec_cmp(struct timespec lhs, struct timespec rhs);
static struct timespec timespec_add(struct timespec lhs, struct timespec rhs);
static struct timespec timespec_sub(struct timespec lhs, struct timespec rhs);
static void timespec_minify(struct timespec *lhs, struct timespec rhs);
static void timespec_maxify(struct timespec *lhs, struct timespec rhs);

static const struct timespec very_long_time = {31536000, 0}; // 1 year
static const struct timespec activity_error = {0, 10000000}; // 10ms
static const struct timespec min_sleep_time = {0, 10000000}; // 10ms
// fire tasks in range (last, current - offset]
static struct timespec last, offset;
// last user activity
static struct timespec last_act;
// if busy, assume user is always active
static bool busy;

void timecalc_init(void) {
    last = (const struct timespec) {0, 0};
    if(clock_gettime(CLOCK_MONOTONIC, &offset) < 0)
        die_perror("clock_gettime");
    last_act = offset;
    busy = false;
}

void timecalc_cycle(struct timespec *timeout,
        struct Task *tasks, unsigned n) {
    struct timespec cur;
    if(clock_gettime(CLOCK_MONOTONIC, &cur) < 0)
        die_perror("clock_gettime");

    struct timespec running = {0, 0};
    for(unsigned i = 0; i < n; i++)
        if(tasks[i].pid)
            timespec_maxify(&running, tasks[i].time);

    struct timespec idle = get_idle_time();
    if(busy)
        idle = (const struct timespec) {0, 0};

    struct timespec activity = timespec_sub(cur, idle);

    if(timespec_cmp(last, running) < 0) {
        // assume new user activity now
        last = running;
        offset = timespec_sub(cur, running);
        activity = cur;
    } else if(timespec_cmp(timespec_sub(activity, last_act), activity_error) > 0) {
        // detected new user activity
        last = running;
        offset = timespec_sub(activity, running);
    }

    last_act = activity;

    struct timespec end = timespec_sub(cur, offset);
    for(unsigned i = 0; i < n; i++)
        if(timespec_cmp(last, tasks[i].time) < 0 &&
                timespec_cmp(tasks[i].time, end) <= 0) {
            execute_task(tasks + i);
            timespec_maxify(&running, tasks[i].time);
        }
    last = end;

    *timeout = very_long_time;
    if(!busy) {
        for(unsigned i = 0; i < n; i++) {
            if(timespec_cmp(tasks[i].time, last) > 0)
                timespec_minify(timeout, timespec_sub(tasks[i].time, last));
            if(timespec_cmp(tasks[i].time, running) > 0)
                timespec_minify(timeout, timespec_sub(tasks[i].time, running));
        }
        timespec_maxify(timeout, min_sleep_time);
    }
}

void timecalc_set_busy(bool b) {
    busy = b;
}
bool timecalc_is_busy(void) {
    return busy;
}

/**
 * Get user idle time using the xscreensaver extension.
 */
static struct timespec get_idle_time(void) {
    Display *display = XOpenDisplay(NULL);
    if(!display)
        die("Cannot open display.\n");
    XScreenSaverInfo *info = XScreenSaverAllocInfo();
    if(!info) {
        XCloseDisplay(display);
        die("Cannot allocate XScreenSaverInfo.\n");
    }
    if(!XScreenSaverQueryInfo(display, XDefaultRootWindow(display), info)) {
        XFree(info);
        XCloseDisplay(display);
        die("X screen saver extension not supported.\n");
    }

    struct timespec idle;
    idle.tv_sec  = info->idle / 1000,
    idle.tv_nsec = info->idle % 1000 * 1000000;

    XFree(info);
    XCloseDisplay(display);
    return idle;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
// lhs > rhs ? 1 : lhs < rhs ? -1 : 0
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
// lhs + rhs
static struct timespec timespec_add(struct timespec lhs, struct timespec rhs) {
    lhs.tv_sec += rhs.tv_sec;
    lhs.tv_nsec += rhs.tv_nsec;
    if(lhs.tv_nsec >= 1000000000) {
        lhs.tv_nsec -= 1000000000;
        lhs.tv_sec += 1;
    }
    return lhs;
}
// lhs - rhs
static struct timespec timespec_sub(struct timespec lhs, struct timespec rhs) {
    lhs.tv_sec -= rhs.tv_sec;
    if(lhs.tv_nsec < rhs.tv_nsec) {
        lhs.tv_nsec += 1000000000;
        lhs.tv_sec -= 1;
    }
    lhs.tv_nsec -= rhs.tv_nsec;
    return lhs;
}
// *lhs = min(*lhs, rhs)
static void timespec_minify(struct timespec *lhs, struct timespec rhs) {
    if(timespec_cmp(*lhs, rhs) > 0)
        *lhs = rhs;
}
// *lhs = max(*lhs, rhs)
static void timespec_maxify(struct timespec *lhs, struct timespec rhs) {
    if(timespec_cmp(*lhs, rhs) < 0)
        *lhs = rhs;
}
#pragma GCC diagnostic pop
