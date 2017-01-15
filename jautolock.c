/*
 * jautolock: fire up programs in case of user inactivity under X
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
#include <confuse.h>
#include <errno.h>
#include <getopt.h>
#include <glob.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "action.h"
#include "die.h"
#include "fifo.h"
#include "timecalc.h"
#include "timespecop.h"

static char *get_config_path();
static int config_validate_time(cfg_t *cfg, cfg_opt_t *opt);
static int config_validate_action(cfg_t *cfg, cfg_opt_t *opt);
static char *concat_strings(char **list, int n);
static int mask_and_signalfd(int signum);
static void prepare_cleanup_fifo();
static void execute_action(struct Action *action);
static pid_t get_dead_child_pid(int sigfd);

static struct option long_options[] = {
    {"config", required_argument, 0, 'c'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};
static cfg_opt_t action_opts[] = {
    CFG_INT("time", 600, CFGF_NONE),
    CFG_STR("command", NULL, CFGF_NODEFAULT),
    CFG_END()
};
static cfg_opt_t opts[] = {
    CFG_SEC("action", action_opts, CFGF_MULTI | CFGF_TITLE),
    CFG_END()
};

int main(int argc, char **argv) {
    char *config_file = NULL;
    while(true) {
        int option_index = 0;
        int opt = getopt_long(argc, argv, "c:h", long_options, &option_index);
        if(opt == -1)
            break;
        switch(opt) {
        case 'c':
            config_file = strdup(optarg);
            if(!config_file)
                die("strdup() failed. Reason: %s\n", strerror(errno));
            break;
        case 'h':
            printf("jautolock © 2017 Pochang Chen\n"
                   "Usage: %s [-c <configfile>] [-h] [<message>]\n", argv[0]);
            return 0;
        }
    }
    if(config_file == NULL)
        config_file = get_config_path();

    cfg_t *config = cfg_init(opts, CFGF_NONE);
    cfg_set_validate_func(config, "action|time", config_validate_time);
    cfg_set_validate_func(config, "action", config_validate_action);
    switch(cfg_parse(config, config_file)) {
    case CFG_FILE_ERROR:
        die("Failed to read config file.\n");
    case CFG_PARSE_ERROR:
        die("Failed to parse config file.\n");
    }
    free(config_file);

    if(optind < argc) {
        char *s = concat_strings(argv + optind, argc - optind);
        int fd = open_fifo_write();
        if(write(fd, s, strlen(s) + 1) < 0)
            die("write() failed. Reason: %s\n", strerror(errno));
        close(fd);
        if(argc - optind >= 2)
            free(s);
        return 0;
    }

    unsigned n_action = cfg_size(config, "action");
    if(n_action == 0)
        die("no actions specified");
    struct Action *actions = calloc(n_action, sizeof(struct Action));
    for(unsigned i = 0; i < n_action; i++) {
        cfg_t *action = cfg_getnsec(config, "action", i);
        actions[i].name = cfg_title(action);
        actions[i].time.tv_sec = cfg_getint(action, "time");
        actions[i].command = cfg_getstr(action, "command");
    }

    int sigfd = mask_and_signalfd(SIGCHLD);
    int fifofd = open_fifo_read();
    prepare_cleanup_fifo();

    timecalc_reset();

    while(true) {
        struct timespec timeout;
        timecalc_sleep(&timeout, actions, n_action);

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sigfd, &readfds);
        FD_SET(fifofd, &readfds);
        int maxfd = sigfd;
        if(maxfd < fifofd)
            maxfd = fifofd;

        if(pselect(maxfd + 1, &readfds, NULL, NULL, &timeout, NULL) < 0)
            die("pselect() failed. Reason: %s\n", strerror(errno));

        if(FD_ISSET(sigfd, &readfds)) {
            int pid = get_dead_child_pid(sigfd);
            for(unsigned i = 0; i < n_action; i++)
                if(actions[i].pid == pid)
                    actions[i].pid = 0;
            timecalc_next_offset(actions, n_action);
        }
        if(FD_ISSET(fifofd, &readfds)) {
            char buf[1024];
            ssize_t sz = read(fifofd, buf, sizeof(buf) - 1);
            if(sz < 0)
                die("read() failed. Reason: %s\n", strerror(errno));

            buf[sz] = '\0';
            if(strcmp(buf, "exit") == 0)
                break;
            if(strncmp(buf, "firenow ", 8) == 0) {
                const char *name = buf + 8;
                for(unsigned i = 0; i < n_action; i++) {
                    if(strcmp(actions[i].name, name) == 0 &&
                            actions[i].pid == 0) {
                        execute_action(actions + i);
                        timecalc_firenow(actions[i].time);
                    }
                }
            }
        }

        struct timespec tbegin, tend;
        timecalc_check_range(&tbegin, &tend);
        for(unsigned i = 0; i < n_action; i++) {
            if(timespec_between_oc(tbegin, actions[i].time, tend)) {
                execute_action(actions + i);
            }
        }
        timecalc_next_offset(actions, n_action);
    }

    free(actions);
    cfg_free(config);
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

// concat list[0] to list[n - 1] together
static char *concat_strings(char **list, int n) {
    if(n == 1)
        return *list;
    char *s = *list;
    for(int i = 1; i < n; i++) {
        char *t;
        if(asprintf(&t, "%s %s", s, list[i]) < 0)
            die("asprintf() failed.");
        if(i > 1)
            free(s);
        s = t;
    }
    return s;
}

static int mask_and_signalfd(int signum) {
    sigset_t mask;
    if(sigemptyset(&mask) < 0)
        die("sigemptyset() failed. Reason: %s\n", strerror(errno));
    if(sigaddset(&mask, signum) < 0)
        die("sigaddset() failed. Reason: %s\n", strerror(errno));
    if(sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
        die("sigprocmask() failed. Reason: %s\n", strerror(errno));
    int fd = signalfd(-1, &mask, SFD_CLOEXEC);
    if(fd < 0)
        die("signalfd() failed. Reason: %s\n", strerror(errno));
    return fd;
}

static void unlink_fifo_atexit_wrapper() {
    unlink_fifo();
}
static void unlink_fifo_signal_wrapper(int sig) {
    unlink_fifo();
    signal(sig, SIG_DFL);
    raise(sig);
}
static void prepare_cleanup_fifo() {
    atexit(unlink_fifo_atexit_wrapper);
    struct sigaction act = {0};
    act.sa_handler = unlink_fifo_signal_wrapper;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}

static pid_t get_dead_child_pid(int sigfd) {
    struct signalfd_siginfo siginfo;
    if(read(sigfd, &siginfo, sizeof(siginfo)) < 0)
        die("read() failed. Reason: %s\n", strerror(errno));
    if(siginfo.ssi_signo != SIGCHLD)
        die("SIGCHLD expected, not %s\n", strsignal(siginfo.ssi_signo));
    pid_t pid = wait(NULL);
    if(pid < 0)
        die("wait() failed. Reason: %s\n", strerror(errno));
    return pid;
}

static void execute_action(struct Action *action) {
    if(action->pid != 0) {
        fprintf(stderr, "WARNING: attempted to fire a running action");
        return;
    }

    int pid = fork();
    if(pid == 0) {
        execlp("sh", "sh", "-c", action->command, NULL);
        _exit(EXIT_FAILURE); // just in case
    }
    if(pid < 0)
        die("fork() failed. Reason: %s\n", strerror(errno));
    action->pid = pid;
}

