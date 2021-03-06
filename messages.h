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
struct Task;
/**
 * Handle messages send by the users.
 * Does not actually exit upon receiving "exit" message.
 * Caller should check for "exit" message.
 *
 * Returns an appropriate free()-able message,
 * intended to be sent back to user.
 */
char *handle_messages(const char *message, struct Task *tasks, unsigned n);
#endif // JAUTOLOCK_MESSAGES_H
