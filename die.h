/*
 * die.h - print message and exit
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
#ifndef JAUTOLOCK_DIE_H
#define JAUTOLOCK_DIE_H
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
/**
 * Print error message to stderr and exit(EXIT_FAILURE).
 */
_Noreturn static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    exit(EXIT_FAILURE);
}
#endif // JAUTOLOCK_DIE_H
