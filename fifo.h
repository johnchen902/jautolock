/*
 * fifo.h - opens FIFO for jautolock
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
#ifndef JAUTOLOCK_FIFO_H
#define JAUTOLOCK_FIFO_H
/**
 * Make a new fifo and open it to read.
 * On success, returns the file descripter.
 * On failure, die.
 */
int open_fifo_read(void);
/**
 * Find an appropriate fifo to write to.
 * On success, returns the file descripter.
 * On failure, die.
 */
int open_fifo_write(void);
/**
 * Unlink the fifo created by open_fifo_read.
 */
void unlink_fifo(void);
#endif // JAUTOLOCK_FIFO_H
