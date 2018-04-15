/*
 * Filename: src/inc/cf.h
 * Project: libcf
 * Brief: Public interface for libcf
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

#ifndef CF_H

#define CF_H 1

#include <stdbool.h>
    // Import type bool
#include <unistd.h>
    // Import type size_t


enum cclass_set {
    CC_UNDEF         = 0x0001,
    CC_CODE          = 0x0002,
    CC_OUTER_STRING  = 0x0004,
    CC_INNER_STRING  = 0x0008,
    CC_OUTER_CHAR    = 0x0010,
    CC_INNER_CHAR    = 0x0020,
    CC_OUTER_COMMENT = 0x0040,
    CC_INNER_COMMENT = 0x0080,
    CC_EOF           = 0x0100,
    CC_ERR           = 0x0200,
    CC_SETMASK       = 0x03FF,
};


/*
 * Data type definitions.
 *
 * A super-character is a pair of int, a character class (code block)
 * and a character within that class.
 *
 *
 * cf_next() takes a mutable array of super-characters.
 * Each call to cf_next() can return 0, 1, or 2 super-characters,
 * depending on whether the latest character is of an ambiguous
 * character class.
 *
 */

struct ccl {
    int ccl;
    int chr;
};

typedef struct ccl ccl_t;

struct cclassv {
    size_t sz;
    size_t len;
    ccl_t  array[0];
};

typedef struct cclassv ccv_t;

struct cf;
typedef struct cf cf_t;

/*
 *
 * Functions for managing a mutable array of super-characters.
 */

extern ccv_t *ccv_new(void);
extern void ccv_delete(ccv_t *ccv);
extern int  ccv_push(ccv_t *ccv, int chr, int cclass);
extern int  ccv_top(ccv_t *ccv, int *rchr, int *rccl);
extern int  ccv_pop(ccv_t *ccv, int *rchr, int *rccl);
extern bool ccv_empty(ccv_t *ccv);
extern const char *decode_cclass(int);


/*
 *
 * Functions for managing the libcf state machine.
 *
 */

extern void cf_init(cf_t *ctx, int cclset);
extern cf_t *cf_new(int cclset);
extern int cf_next(cf_t *ctx, ccv_t *rccv, int chr);

#endif /* CF_H */
