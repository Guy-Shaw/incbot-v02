/*
 * Filename: src/inc/dict.h
 * Project: libincbot
 * Brief: Public interface for data type, 'dict_t'
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

#ifndef DICT_H

#define DICT_H 1

/*
 * This implementation of "dictionary" is specialized for symbol
 * tables that get built up as new symbols arrive, but
 * symbols do not get removed.  This is a common use.
 * Because there is no support for deletion of a symbol,
 * symbols can just be appended.
 *
 * The dictionary grows as needed, one segment at a time.
 * Access is either by symbol table lookup or by a symbol reference
 * number.  Access by some sort of slot number is possible,
 * because symbols do not get deleted _and_ they are never relocated.
 * This means that views of the symbol table can use the reference
 * numbers rather than pointers to symbol table entries.
 *
 * This also means that the whole symbol table can be serialized,
 * "compiled", saved to durable storage, then reloaded, with a
 * minimum of relocation.  It also means that it can be placed
 * in shared memory and accessed by different processes, with a minimum
 * of address swizzling, because so much of what would ordinarily
 * be pointers would be just entry numbers, which get translated
 * into memory segment offsets, anyway.
 *
 * It also means that, even on a 64-bit ISA, we could use 32-bit
 * symbol-table slot numbers, as long as we are willing to live
 * with the restriction that we can only have 4G symbols.
 *
 * Access by slot number to a segmented array is pretty much
 * just a 2-dimensional, expandable, Iliffe vector.
 *
 */


struct dict {
    char ***sv;		// Segment vector
    size_t sz;		// Allocated capacity, number of entries (not bytes)
    size_t len;		// Number of entries occupied
    void   *hashtable;
};

typedef struct dict dict_t;

/*
 * Implementation note:
 *   We rely on undef_symnr being 0, rather than, say, (size_t)(-1),
 *   simply because we can then use calloc() to get a properly initialized
 *   hash table.
 *
 */
static const size_t undef_symnr = (size_t)(0);

static const size_t dict_segment_size = 1024;

extern void dict_init(dict_t *dict);
extern dict_t *dict_new(void);
extern size_t dict_add(dict_t *dict, const char *s);
extern size_t dict_find(dict_t *dict, const char *s);
extern char *dict_getname_str(dict_t *dict, const char *s);
extern char *dict_getname_nr(dict_t *dict, size_t pos);

#endif /* DICT_H */
