/*
 * action.c - actions that may be fired by jautolock on inactivity
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
#include "action.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "die.h"

void execute_action(struct Action *action) {
    if(action->pid != 0) {
        fprintf(stderr, "WARNING: attempted to fire a running action");
        return;
    }

    int pid = fork();
    if(pid == 0) {
        execlp("sh", "sh", "-c", action->command, NULL);
        _exit(EXIT_FAILURE);
    }
    if(pid < 0)
        die("fork() failed. Reason: %s\n", strerror(errno));
    action->pid = pid;
}

