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
/**
 * A tasks that may be fired by jautolock.
 * time: inactivity time before this program is fired
 * name: name of the task (used to fired it immediately)
 * command: command to run
 * pid: if zero, the task is not running
 *      otherwise, the task is running, and has this pid
 */
struct Task {
    struct timespec time;
    const char *name;
    const char *command;
    pid_t pid;
};
/**
 * Forks and execute the specified task.
 *
 * The task should not be running (i.e. task->pid should be 0).
 * If this condition is not hold, the task will not be run
 * and a warning will be printed.
 */
void execute_task(struct Task *task);
#endif // TASKS_H
