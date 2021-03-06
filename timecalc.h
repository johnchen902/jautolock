/*
 * timecalc.h - time related calculations for jautolock
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
#ifndef JAUTOLOCK_TIMECALC_H
#define JAUTOLOCK_TIMECALC_H
/**
 * Forward declarations. See "tasks.h" and <time.h> respectively.
 */
struct Task;
struct timespec;
/**
 * Call this before calling any other methods here.
 */
void timecalc_init(void);
/**
 * Fire tasks that have timed out and determine
 * appropriate time to sleep so the next task
 * will be run on time.
 *
 * The maximum sleep time is 365 days,
 * and the mimimum is 10 milliseconds.
 * TODO add configuration for this
 * TODO somehow returns "infinity" sleep time
 */
void timecalc_cycle(struct timespec *sleep_time,
        struct Task *tasks, unsigned n);
/**
 * If busy, assume user is always active.
 */
void timecalc_set_busy(_Bool busy);
_Bool timecalc_is_busy(void);
#endif // JAUTOLOCK_TIMECALC_H
