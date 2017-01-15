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
#ifndef TIMECALC_H
#define TIMECALC_H
struct Action;
struct timespec;
void timecalc_reset();
void timecalc_sleep(struct timespec *t, const struct Action *actions, unsigned n);
void timecalc_next_offset(const struct Action *actions, unsigned n);
void timecalc_check_range(struct timespec *begin, struct timespec *end);
void timecalc_firenow(struct timespec what);
#endif // TIMECALC_H
