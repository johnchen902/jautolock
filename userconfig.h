/*
 * userconfig.h - find, read and parse user configuration file
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
#ifndef USERCONFIG_H
#define USERCONFIG_H
/**
 * Forward declaration. See task.h
 */
struct Task;
/**
 * Read the configuration file specified.
 * Search in default locations if config_file is NULL.
 * Die if the config contains errors.
 * Use empty config if the config file is not found.
 */
void read_config(const char *config_file);
/**
 * Get a list of tasks from previously read config.
 * Returns the number of tasks.
 * The list of tasks is put in *tasks_ptr,
 * which should be free()-d when appropriate.
 *
 * It is undefined behavior to pass in NULL.
 */
unsigned get_tasks(struct Task **tasks_ptr);
/**
 * Free up the config previously read by read_config().
 */
void free_config(void);
#endif // USERCONFIG_H
