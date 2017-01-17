/*
 * tasks.h - tasks that may be fired by jautolock on inactivity
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
#ifndef TASKS_H
#define TASKS_H
#include <time.h>
struct Task {
    struct timespec time;
    const char *name;
    const char *command;
    pid_t pid;
};
void execute_task(struct Task *task);
#endif // TASKS_H
