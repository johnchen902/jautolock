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
#ifndef JAUTOLOCK_USERCONFIG_H
#define JAUTOLOCK_USERCONFIG_H
#include <confuse.h>
struct Task;
/**
 * Read the specfied configuration file.
 * Search for one if config_file is NULL.
 */
cfg_t *read_config(const char *config_file);
/**
 * Get a list of tasks from config.
 * Returns the number of tasks.
 * The list of tasks is put in *tasks_ptr.
 */
unsigned get_tasks(cfg_t *config, struct Task **tasks_ptr);
#endif // JAUTOLOCK_USERCONFIG_H
