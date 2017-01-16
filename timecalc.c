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
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "action.h"
#include "die.h"
#include "timespecop.h"

static struct timespec get_idle_time();

static const struct timespec activity_error = {0, 10000000}; // 10ms
static const struct timespec min_sleep_time = {0, 10000000}; // 10ms
// fire actions in range (last, current - offset]
static struct timespec last, offset;
// last user activity
static struct timespec last_act;

void timecalc_init() {
    last = (const struct timespec) {0, 0};
    if(clock_gettime(CLOCK_MONOTONIC, &offset) < 0)
        die("clock_gettime() failed. Reason: %s\n", strerror(errno));
    last_act = offset;
}

void timecalc_cycle(struct timespec *timeout,
        struct Action *actions, unsigned n) {
    struct timespec cur;
    if(clock_gettime(CLOCK_MONOTONIC, &cur) < 0)
        die("clock_gettime() failed. Reason: %s\n", strerror(errno));

    struct timespec running = {0, 0};
    for(unsigned i = 0; i < n; i++)
        if(actions[i].pid)
            timespec_maxify(&running, actions[i].time);

    struct timespec idle = get_idle_time();
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
        if(timespec_cmp(last, actions[i].time) < 0 &&
                timespec_cmp(actions[i].time, end) <= 0) {
            execute_action(actions + i);
            timespec_maxify(&running, actions[i].time);
        }
    last = end;

    *timeout = (const struct timespec) {365 * 86400, 0};
    for(unsigned i = 0; i < n; i++) {
        if(timespec_cmp(actions[i].time, last) > 0)
            timespec_minify(timeout, timespec_sub(actions[i].time, last));
        if(timespec_cmp(actions[i].time, running) > 0)
            timespec_minify(timeout, timespec_sub(actions[i].time, running));
    }
    timespec_maxify(timeout, min_sleep_time);
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
