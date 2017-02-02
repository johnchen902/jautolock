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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "die.h"
#include "tasks.h"
#include "timecalc.h"

static char *strjoin(char *first, const char *second);
static char *handle_now(char *arg, struct Task *tasks, unsigned n);
static char *handle_busy(char *arg, struct Task *tasks, unsigned n);
static char *handle_unbusy(char *arg, struct Task *tasks, unsigned n);
static char *handle_exit(char *arg, struct Task *tasks, unsigned n);

struct {
    const char *const command;
    char *(*const handler) (char *arg, struct Task *tasks, unsigned n);
} actions[] = {
    {"now", handle_now},
    {"busy", handle_busy},
    {"unbusy", handle_unbusy},
    {"exit", handle_exit},
};

char *handle_messages(const char *cmessage, struct Task *tasks, unsigned n) {
    char *message = strdup(cmessage);
    if(!message)
        die_perror("strdup");

    char *arg = strchr(message, ' ');
    if(arg)
        *arg++ = '\0';
    else
        arg = strchr(message, '\0');

    char *response = strdup("Message received.");
    if(!response)
        die_perror("strdup");

    bool understood = false;
    for(unsigned i = 0; i < sizeof(actions) / sizeof(actions[0]); i++)
        if(strcmp(message, actions[i].command) == 0) {
            understood = true;
            char *s = (actions[i].handler)(arg, tasks, n);
            if(s) {
                response = strjoin(response, s);
                free(s);
            }
        }

    if(!understood)
        response = strjoin(response, "However I don't understand it.");

    free(message);
    return response;
}

/**
 * strjoin s t = s ++ "\n" ++ t
 *
 * First string will be freed.
 * Returns a freeable string.
 */
static char *strjoin(char *first, const char *second) {
    first = realloc(first, strlen(first) + strlen(second) + 2);
    if(!first)
        die_perror("realloc");
    strcat(first, "\n");
    strcat(first, second);
    return first;
}

/**
 * Execute the task specified in arg immediately.
 */
static char *handle_now(char *arg, struct Task *tasks, unsigned n) {
    if(!*arg)
        return strdup("\"now\" expect one argument.");
    bool matched = false;
    bool fired = false;
    for(unsigned i = 0; i < n; i++)
        if(strcmp(tasks[i].name, arg) == 0) {
            matched = true;
            if(tasks[i].pid == 0) {
                fired = true;
                execute_task(tasks + i);
            }
        }
    if(!matched)
        return strdup("No task has such name.");
    if(!fired)
        return strdup("The task is already running.");
    return strdup("Task fired.");
}

/**
 * See timecalc_set_busy.
 */
static char *handle_busy(char *arg, struct Task *tasks, unsigned n) {
    (void) arg, (void) tasks, (void) n;
    timecalc_set_busy(true);
    return strdup("You're assumed to be busy.");
}
static char *handle_unbusy(char *arg, struct Task *tasks, unsigned n) {
    (void) arg, (void) tasks, (void) n;
    timecalc_set_busy(false);
    return strdup("You're no longer assumed to be busy.");
}

// Just say "OK, I'll exit"
static char *handle_exit(char *arg, struct Task *tasks, unsigned n) {
    (void) arg, (void) tasks, (void) n;
    if(*arg)
        return strdup("\"exit\" expect no argument.");
    return strdup("Will exit.");
}
