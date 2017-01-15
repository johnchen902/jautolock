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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "die.h"

static const char *getfifodir();
static int filter(const struct dirent *entry);

static char *fifo_path = NULL;

int open_fifo_read() {
    if(fifo_path)
        die("Only one fifo can exists\n");
    if(asprintf(&fifo_path,
            "%s/jautolock-%d.fifo", getfifodir(), getpid()) < 0)
        die("asprintf() failed\n");
    if(mkfifo(fifo_path, 0600) < 0)
        die("mkfifo() failed. Reason: %s\n", strerror(errno));
    int fd = open(fifo_path, O_RDWR);
    if(fd < 0)
        die("open() failed. Reason: %s\n", strerror(errno));
    return fd;
}
int open_fifo_write() {
    const char *dirp = getfifodir();
    struct dirent **namelist;
    int n = scandir(dirp, &namelist, filter, alphasort);
    if(n < 0)
        die("scandir() failed. Reason: %s\n", strerror(errno));
    if(n == 0)
        die("no suitable FIFO is found.\n");

    int dirfd = open(dirp, O_RDONLY), fd;
    if(dirfd < 0)
        die("open() failed. Reason: %s\n", strerror(errno));
    for(int i = 0; i < n; i++) {
        fd = openat(dirfd, namelist[i]->d_name, O_WRONLY | O_NONBLOCK);
        if(fd >= 0)
            goto found;
    }
    die("no suitable FIFO is found.\n");
found:
    close(dirfd);
    free(namelist);
    return fd;
}
void unlink_fifo() {
    if(fifo_path) {
        unlink(fifo_path);
        free(fifo_path);
        fifo_path = NULL;
    }
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
