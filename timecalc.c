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
#include "action.h"
#include "die.h"
#include "timespecop.h"
#include <X11/extensions/scrnsaver.h>
#include <errno.h>
#include <string.h>
#include <time.h>

static struct timespec get_idle_time();

static const struct timespec actdiff_threshold = {0, 10000000}; // 10ms
static const struct timespec min_sleep_time    = {0, 10000000}; // 10ms
static struct timespec act;    // last user activity time
static struct timespec last;   // last time checked
static struct timespec offset;     // TODO what is this
static struct timespec nextoffset; // TODO what is this

void timecalc_reset() {
    if(clock_gettime(CLOCK_MONOTONIC, &last) < 0)
        die("clock_gettime() failed. Reason: %s\n", strerror(errno));
    act = last;
    offset.tv_sec = offset.tv_nsec = 0;
    nextoffset.tv_sec = nextoffset.tv_nsec = 0;
}

void timecalc_sleep(struct timespec *timeout,
        const struct Action *actions, unsigned n) {
    timeout->tv_sec = 365 * 86400; // almost infinity
    timeout->tv_nsec = 0;
    for(unsigned i = 0; i < n; i++) {
        struct timespec t1 =
                timespec_sub(timespec_add(act, actions[i].time), offset);
        timespec_minify_pos(timeout, t1, last);
        timespec_minify_pos(timeout, actions[i].time, nextoffset);
    }
    timespec_maxify(timeout, min_sleep_time);
}

void timecalc_next_offset(const struct Action *actions, unsigned n) {
    nextoffset.tv_sec = nextoffset.tv_nsec = 0;
    for(unsigned i = 0; i < n; i++)
        if(actions[i].pid)
            timespec_maxify(&nextoffset, actions[i].time);
}

void timecalc_check_range(struct timespec *begin, struct timespec *end) {
    struct timespec cur;
    if(clock_gettime(CLOCK_MONOTONIC, &cur) < 0)
        die("clock_gettime() failed. Reason: %s\n", strerror(errno));
    struct timespec nextact = timespec_sub(cur, get_idle_time());
    if(timespec_cmp(timespec_sub(nextact, act), actdiff_threshold) > 0) {
        // new user activity
        act = nextact;
        offset = nextoffset;
        last = act;
    }
    *begin = timespec_add(timespec_sub(last, act), offset);
    *end   = timespec_add(timespec_sub(cur,  act), offset);
    last = cur;
}

void timecalc_firenow(struct timespec when) {
    if(timespec_cmp(when, nextoffset) <= 0)
        return;
    struct timespec cur;
    if(clock_gettime(CLOCK_MONOTONIC, &cur) < 0)
        die("clock_gettime() failed. Reason: %s\n", strerror(errno));
    offset = nextoffset = when;
    act = last = cur;
}

static struct timespec get_idle_time() {
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
