/*
 * messages.h - handling messages recieved via FIFO
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
#ifndef JAUTOLOCK_MESSAGES_H
#define JAUTOLOCK_MESSAGES_H
/**
 * Forward declaration. See "tasks.h"
 */
struct Task;
/**
 * Handle messages send by the users.
 * Does not handle "exit" message because it don't know how.
 *
 * May modify message for its convenience but will not free it.
 */
void handle_messages(char *message, struct Task *tasks, unsigned n);
#endif // JAUTOLOCK_MESSAGES_H
