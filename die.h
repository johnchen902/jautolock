/*
 * die.h - print error message and exit
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
/**
 * Print error message to stderr and exit(EXIT_FAILURE).
 * (fprintf version)
 */
_Noreturn void die(const char *fmt, ...);
/**
 * Print error message to stderr and exit(EXIT_FAILURE).
 * (perror version)
 */
_Noreturn void die_perror(const char *s);
#endif // JAUTOLOCK_DIE_H
