/*
 * timespecop.h - some basic operations on struct timespec
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
#ifndef TIMESPECOP_H
#define TIMESPECOP_H
#include <time.h>
__attribute__((unused)) static int timespec_cmp(struct timespec lhs,
        struct timespec rhs) {
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
__attribute__((unused)) static struct timespec timespec_add(struct timespec lhs,
        struct timespec rhs) {
    lhs.tv_sec += rhs.tv_sec;
    lhs.tv_nsec += rhs.tv_nsec;
    if(lhs.tv_nsec >= 1000000000) {
        lhs.tv_nsec -= 1000000000;
        lhs.tv_sec += 1;
    }
    return lhs;
}
__attribute__((unused)) static struct timespec timespec_sub(struct timespec lhs,
        struct timespec rhs) {
    lhs.tv_sec -= rhs.tv_sec;
    if(lhs.tv_nsec < rhs.tv_nsec) {
        lhs.tv_nsec += 1000000000 - rhs.tv_nsec;
        lhs.tv_sec -= 1;
    } else {
        lhs.tv_nsec -= rhs.tv_nsec;
    }
    return lhs;
}
__attribute__((unused)) static void timespec_minify(struct timespec *lhs,
        struct timespec rhs) {
    if(timespec_cmp(*lhs, rhs) > 0)
        *lhs = rhs;
}
__attribute__((unused)) static void timespec_maxify(struct timespec *lhs,
        struct timespec rhs) {
    if(timespec_cmp(*lhs, rhs) < 0)
        *lhs = rhs;
}
__attribute__((unused)) static void timespec_minify_pos(struct timespec *lhs,
        struct timespec a, struct timespec b) {
    if(timespec_cmp(a, b) > 0)
        timespec_minify(lhs, timespec_sub(a, b));
}
__attribute__((unused)) static _Bool timespec_between_oc(struct timespec min,
        struct timespec x, struct timespec max) {
    return timespec_cmp(min, x) < 0 && timespec_cmp(x, max) <= 0;
}
#endif // TIMESPECOP_H
