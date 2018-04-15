/*
 * Filename: src/inc/cf-impl.h
 * Project: libcf
 * Brief: definitions private to the implementation of libc
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

#ifndef CF_IMPL_H

#ifndef CF_H
#error "Require cf.h"
#endif

#define CF_IMPL_H 1

struct cf {
    int err;
    int state;
    enum cclass_set cclass;
};

enum state {
    S_START,
    S_START_DQUOTE,
    S_DQUOTE_ESCAPE,
    S_START_SLASH,
    S_SLASH_STAR,
    S_SLASH_STAR_STAR,
    S_START_SQUOTE,
    S_SQUOTE_ESCAPE,
    S_INCHAR,
    S_COMMENT_EOL,	// Alias S_SLASH_SLASH
    S_EOF,
};

extern const char *decode_state(int state);

#endif /* CF_IMPL_H */
