/*
 * userconfig.c - find, read and parse user configuration file
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
#include "userconfig.h"
#include <basedir_fs.h>
#include <confuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "die.h"
#include "tasks.h"

static char *get_config_path(const char *const_config_path);
static int parse_time(const char *s, struct timespec *t);
static int config_validate_time(cfg_t *cfg, cfg_opt_t *opt);
static int config_validate_task(cfg_t *cfg, cfg_opt_t *opt);

static cfg_opt_t task_opts[] = {
    CFG_STR("time", "600s", CFGF_NONE),
    CFG_STR("command", NULL, CFGF_NODEFAULT),
    CFG_END()
};
static cfg_opt_t opts[] = {
    CFG_SEC("task", task_opts, CFGF_MULTI | CFGF_TITLE),
    CFG_END()
};

cfg_t *read_config(const char *const_config_file) {
    cfg_t *config = cfg_init(opts, CFGF_NONE);
    cfg_set_validate_func(config, "task|time", config_validate_time);
    cfg_set_validate_func(config, "task", config_validate_task);

    char *config_file = get_config_path(const_config_file);
    if(*config_file == '\0') {
        fprintf(stderr,
                "No configuration file found.\n"
                "Using default configuration instead.\n");
        free(config_file);
        return config;
    }

    switch(cfg_parse(config, config_file)) {
    case CFG_FILE_ERROR:
        fprintf(stderr,
                "Configuration file (%s) cannot be opened for reading.\n"
                "Using default configuration instead.\n",
                config_file);
        break;
    case CFG_PARSE_ERROR:
        die("Failed to parse configuration file.\n");
    }
    free(config_file);
    return config;
}

unsigned get_tasks(cfg_t *config, struct Task **tasks_ptr) {
    unsigned n = cfg_size(config, "task");
    *tasks_ptr = calloc(n, sizeof(struct Task));
    for(unsigned i = 0; i < n; i++) {
        cfg_t *task = cfg_getnsec(config, "task", i);
        (*tasks_ptr)[i].name = cfg_title(task);
        parse_time(cfg_getstr(task, "time"), &(*tasks_ptr)[i].time);
        (*tasks_ptr)[i].command = cfg_getstr(task, "command");
    }
    return n;
}

// if const_config_path is not NULL, return a freeable copy
// otherwise return default configuration path
static char *get_config_path(const char *const_config_path) {
    if(const_config_path)
        return strdup(const_config_path);
    return xdgConfigFind("jautolock/config", NULL);
}

// parse time duration of format \(\d+\(d\|h\|m\|s\|ms\|ns\)\)+
static int parse_time(const char *s, struct timespec *t) {
    if(!*s)
        return -1;
    t->tv_sec = t->tv_nsec = 0;
    while(*s) {
        char *ep;
        long x = strtol(s, &ep, 10);
        if(ep == s)
            return -1;
        s = ep;
        if(!*s)
            return -1;
        switch(*s) {
        case 'd':
            t->tv_sec += x * 86400;
            s++;
            break;
        case 'h':
            t->tv_sec += x * 3600;
            s++;
            break;
        case 'm':
            if(s[1] == 's') {
                t->tv_nsec += x * ((int) 1e6);
                s += 2;
                break;
            }
            t->tv_sec += x * 60;
            s++;
            break;
        case 's':
            t->tv_sec += x;
            s++;
            break;
        case 'n':
            if(s[1] != 's')
                return -1;
            t->tv_nsec += x;
            s += 2;
            break;
        default:
            return -1;
        }
    }
    t->tv_sec += t->tv_nsec / ((int) 1e9);
    t->tv_nsec %= ((int) 1e9);
    if(t->tv_nsec < 0) {
        t->tv_sec--;
        t->tv_nsec += ((int) 1e9);
    }
    return 0;
}

// validate the time option (must be positive)
static int config_validate_time(cfg_t *cfg, cfg_opt_t *opt) {
    const char *s = cfg_opt_getnstr(opt, 0);
    struct timespec t;
    if(parse_time(s, &t) < 0) {
        cfg_error(cfg, "bad time format");
        return -1;
    }
    if(t.tv_sec < 0) {
        cfg_error(cfg, "negative time");
        return -1;
    }
    if(t.tv_sec == 0 && t.tv_nsec == 0) {
        cfg_error(cfg, "zero time");
        return -1;
    }
    return 0;
}
// validate the task section (command option required)
static int config_validate_task(cfg_t *cfg, cfg_opt_t *opt) {
    cfg_t *task = cfg_opt_getnsec(opt, cfg_opt_size(opt) - 1);
    if (cfg_size(task, "command") == 0) {
        cfg_error(cfg, "missing required option 'command' in task");
        return -1;
    }
    return 0;
}

