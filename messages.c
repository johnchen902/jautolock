/*
 * messages.c - handling messages recieved via FIFO
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
#include "messages.h"
#include <string.h>
#include "tasks.h"

static void handle_now(char *arg, struct Task *tasks, unsigned n);

struct {
    const char *const command;
    void (*const handler) (char *arg, struct Task *tasks, unsigned n);
} actions[] = {
    {"now", handle_now}
};

void handle_messages(char *message, struct Task *tasks, unsigned n) {
    char *arg = strchr(message, ' ');
    if(arg)
        *arg++ = '\0';
    else
        arg = strchr(message, '\0');

    for(unsigned i = 0; i < sizeof(actions) / sizeof(actions[0]); i++)
        if(strcmp(message, actions[i].command) == 0)
            (actions[i].handler)(arg, tasks, n);
}

static void handle_now(char *arg, struct Task *tasks, unsigned n) {
    for(unsigned i = 0; i < n; i++)
        if(strcmp(tasks[i].name, arg) == 0 && tasks[i].pid == 0)
            execute_task(tasks + i);
}
