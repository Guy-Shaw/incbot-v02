/*
 * Filename: src/inc/incbot.h
 * Project: incbot
 * Library: libincbot
 * Brief: Public interface for libincbot
 *
 * Copyright (C) 2016 Guy Shaw
 * Written by Guy Shaw <gshaw@acm.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INCBOT_H

#define INCBOT_H 1

#include <cscript.h>

extern int  read_config_file(const char *path);
extern void init_tables(void);
extern int  read_id_table_file(const char *path);
extern int  read_id_tables(void);
extern int  incbot_src_file(const char *fname);
extern void show_includes(void);
extern int  trace_identifier(const char *);

#endif /* INCBOT_H */
