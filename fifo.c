/*
 * fifo.c - opens FIFO for jautolock
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
#include "fifo.h"
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const char *getfifodir();
static int filter(const struct dirent *entry);

static char *fifo_path = NULL;

int open_fifo_read() {
    if(fifo_path) {
        errno = EBUSY;
        return -1;
    }
    if(asprintf(&fifo_path,
            "%s/jautolock-%d.fifo", getfifodir(), getpid()) < 0) {
        fifo_path = NULL;
        return -1;
    }
    if(mkfifo(fifo_path, 0600) < 0) {
        int savederrno = errno;
        free(fifo_path);
        fifo_path = NULL;
        errno = savederrno;
        return -1;
    }
    int fd = open(fifo_path, O_RDWR);
    if(fd < 0) {
        int savederrno = errno;
        unlink_fifo();
        errno = savederrno;
        return -1;
    }
    return fd;
}
int open_fifo_write() {
    const char *dirp = getfifodir();
    struct dirent **namelist;
    int n = scandir(dirp, &namelist, filter, alphasort);
    if(n < 0)
        return -1;
    if(n == 0) {
        free(namelist);
        errno = ENOENT;
        return -1;
    }

    int dirfd = open(dirp, O_RDONLY), fd = -1;
    for(int i = 0; i < n; i++) {
        fd = openat(dirfd, namelist[i]->d_name, O_WRONLY | O_NONBLOCK);
        if(fd >= 0)
            break;
    }
    close(dirfd);
    free(namelist);
    if(fd < 0) {
        errno = ENOENT;
        return -1;
    }
    return fd;
}
int unlink_fifo() {
    if(fifo_path) {
        unlink(fifo_path);
        free(fifo_path);
        fifo_path = NULL;
    }
    return 0;
}

static const char *getfifodir() {
    const char *xdg = getenv("XDG_RUNTIME_DIR");
    if(xdg)
        return xdg;
    return "/tmp";
}
static int filter(const struct dirent *entry) {
#ifdef _DIRENT_HAVE_D_TYPE
    if(entry->d_type != DT_FIFO && entry->d_type != DT_UNKNOWN)
        return 0;
#endif
    // TODO more sophisticated filter
    if(strncmp(entry->d_name, "jautolock-", 10) != 0)
        return 0;
    return 1;
}
