/*
 * config.c - handles config file
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
#include "config.h"
#include <confuse.h>
#include <errno.h>
#include <glob.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "action.h"
#include "die.h"

static char *get_config_path();
static int config_validate_time(cfg_t *cfg, cfg_opt_t *opt);
static int config_validate_action(cfg_t *cfg, cfg_opt_t *opt);

static cfg_opt_t action_opts[] = {
    CFG_INT("time", 600, CFGF_NONE),
    CFG_STR("command", NULL, CFGF_NODEFAULT),
    CFG_END()
};
static cfg_opt_t opts[] = {
    CFG_SEC("action", action_opts, CFGF_MULTI | CFGF_TITLE),
    CFG_END()
};

static cfg_t *config;

void read_config(char *config_file) {
    if(config_file == NULL)
        config_file = get_config_path();

    config = cfg_init(opts, CFGF_NONE);
    cfg_set_validate_func(config, "action|time", config_validate_time);
    cfg_set_validate_func(config, "action", config_validate_action);
    switch(cfg_parse(config, config_file)) {
    case CFG_FILE_ERROR:
        die("Failed to read config file.\n");
    case CFG_PARSE_ERROR:
        die("Failed to parse config file.\n");
    }
    free(config_file);
}

unsigned get_actions(struct Action **actions_ptr) {
    unsigned n_action = cfg_size(config, "action");
    *actions_ptr = calloc(n_action, sizeof(struct Action));
    for(unsigned i = 0; i < n_action; i++) {
        cfg_t *action = cfg_getnsec(config, "action", i);
        (*actions_ptr)[i].name = cfg_title(action);
        (*actions_ptr)[i].time.tv_sec = cfg_getint(action, "time");
        (*actions_ptr)[i].command = cfg_getstr(action, "command");
    }
    return n_action;
}

void free_config() {
    cfg_free(config);
    config = NULL;
}

// Copied from i3status
static bool path_exists(const char *path) {
    struct stat buf;
    return stat(path, &buf) == 0;
}
static char *resolve_tilde(const char *path) {
    char *tail = strchr(path, '/');
    char *head = strndup(path, tail ? (size_t)(tail - path) : strlen(path));
    glob_t globbuf;
    int res = glob(head, GLOB_TILDE, NULL, &globbuf);
    free(head);
    char *result = NULL;
    if(res == GLOB_NOMATCH || globbuf.gl_pathc != 1) {
        /* no match, or many wildcard matches are bad */
        result = strdup(path);
        if(!result)
            die("strdup() failed. Reason: %s\n", strerror(errno));
    } else if(res != 0) {
        die("glob() failed");
    } else {
        head = globbuf.gl_pathv[0];
        result = calloc(strlen(head) + (tail ? strlen(tail) : 0) + 1, 1);
        if(!result)
            die("calloc() failed. Reason: %s\n", strerror(errno));
        strncpy(result, head, strlen(head));
        if(tail)
            strncat(result, tail, strlen(tail));
    }
    globfree(&globbuf);

    return result;
}
static char *get_config_path(void) {
    {
        // 1: check for $XDG_CONFIG_HOME/jautolock/config
        char *xdg_config_home = getenv("XDG_CONFIG_HOME");
        if(!xdg_config_home)
            xdg_config_home = "~/.config";

        xdg_config_home = resolve_tilde(xdg_config_home);
        char *config_path;
        if(asprintf(&config_path, "%s/jautolock/config", xdg_config_home) == -1)
            die("asprintf() failed");
        free(xdg_config_home);

        if(path_exists(config_path))
            return config_path;
        free(config_path);
    }

    {
        // 2: check the traditional path under the home directory
        char *config_path = resolve_tilde("~/.jautolock.conf");
        if(path_exists(config_path))
            return config_path;
        free(config_path);
    }

    // 3: check for $XDG_CONFIG_DIRS/jautolock/config
    char *xdg_config_dirs = getenv("XDG_CONFIG_DIRS");
    if(!xdg_config_dirs)
        xdg_config_dirs = "/etc/xdg";
    char *buf = strdup(xdg_config_dirs);
    if(!buf)
        die("strdup() failed. Reason: %s\n", strerror(errno));
    for(char *tok = strtok(buf, ":"); tok != NULL; tok = strtok(NULL, ":")) {
        tok = resolve_tilde(tok);
        char *config_path;
        if(asprintf(&config_path, "%s/jautolock/config", tok) == -1)
            die("asprintf() failed");
        free(tok);
        if(path_exists(config_path)) {
            free(buf);
            return config_path;
        }
        free(config_path);
    }
    free(buf);

    // 4: check the traditional path under /etc
    char *config_path = "/etc/jautolock.conf";
    if(path_exists(config_path)) {
        config_path = strdup(config_path);
        if(!config_path)
            die("strdup() failed. Reason: %s\n", strerror(errno));
        return config_path;
    }

    die("Unable to find the configuration file (looked at "
        "~/.jautolock.conf, $XDG_CONFIG_HOME/jautolock/config, "
        "/etc/jautolock.conf and $XDG_CONFIG_DIRS/jautolock/config)");
    return NULL;
}

// validate the time option (must be positive)
static int config_validate_time(cfg_t *cfg, cfg_opt_t *opt) {
    int value = cfg_opt_getnint(opt, 0);
    if(value < 0) {
        cfg_error(cfg, "negative time %d", value);
        return -1;
    }
    return 0;
}
// validate the action section (command option required)
static int config_validate_action(cfg_t *cfg, cfg_opt_t *opt) {
    cfg_t *action = cfg_opt_getnsec(opt, cfg_opt_size(opt) - 1);
    if (cfg_size(action, "command") == 0) {
        cfg_error(cfg, "missing required option 'command' in action");
        return -1;
    }
    return 0;
}

