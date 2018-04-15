/*
 * Filename: src/libincbot/dict.c
 * Project: incbot
 * Library: libincbot
 * Brief: Implementation of data type, 'dict_t', a dictionary
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <stdio.h>
    // Import fprintf()
    // Import var stderr
#include <stdlib.h>
    // Import exit()
#include <string.h>
    // Import strcmp()
    // Import strdup()
#include <unistd.h>
    // Import exit()
    // Import type size_t
#include <stdint.h>
    // Import type uint32_t

#include <cscript.h>
#include <dict.h>

#ifdef TEST_SMALL

static size_t config_hashmap_init_size = 11;
#define HASHMAP_SET_ASSOCIATIVITY 2
#define OVFL_SEGMENT_SIZE 8

#else

static size_t config_hashmap_init_size = 11; // 10009
#define HASHMAP_SET_ASSOCIATIVITY 2
#define OVFL_SEGMENT_SIZE 256

#endif

static const bool upsert_true  = true;
static const bool upsert_false = false;

static bool config_use_hashtable = true;


// sym_poison probably ought to be defined to be something
// more genuinely poisonous.
// Also, it should be a value that somehow enforces its immutability.
// 
static const char *sym_poison = "*POISON*";

static const size_t undef_ovflnr = 0;

// START   dict-impl.h

void dict_grow(dict_t *dict);
size_t dict_append_symbol(dict_t *dict, const char *s);

// END     dict-impl.h


void
dict_init(dict_t *dict)
{
    dict->sz   = dict_segment_size;
    dict->sv = (char ***)guard_malloc(sizeof (char **));
    dict->sv[0] = (char **)guard_malloc(dict_segment_size * sizeof (char *));

    // Reserve symnr 0 so it can be used for the null value for symbol numbers.
    dict->sv[0][0] = (char *)sym_poison;
    dict->len = 1;
    dict->hashtable = NULL;
}

dict_t *
dict_new()
{
    dict_t *dict;

    dict = (dict_t *)guard_malloc(sizeof (dict_t));
    dict_init(dict);
    return (dict);
}

/*
 * return position in array of dictionary entries.
 * use dict_get(dict, sym)  to get symbol name.
 *
 */

// ============== Start  hashmap.h


struct hashent {
    size_t h;
    size_t symnr;
};

typedef struct hashent hashent_t;

struct ovfl {
    size_t ov_symnr;
    size_t ov_next;
};

typedef struct ovfl ovfl_t;

struct hashbkt {
    hashent_t ent[HASHMAP_SET_ASSOCIATIVITY];
    size_t chain;
};

typedef struct hashbkt hashbkt_t;

struct hashmap {
    hashbkt_t *tbl;
    size_t tbl_sz;
    ovfl_t *ovfl;
    size_t ovfl_sz;
    size_t ovfl_len;
};

typedef struct hashmap hashmap_t;


// ==================== End hashmap.h


static hashmap_t *
hashmap_new(size_t tbl_sz)
{
    if (tbl_sz == 0) {
        tbl_sz = config_hashmap_init_size;
    }
    hashmap_t *map;
    map = (hashmap_t *) guard_malloc(sizeof (hashmap_t));
    map->tbl = (hashbkt_t *) guard_calloc(tbl_sz, sizeof (hashbkt_t));
    map->tbl_sz = tbl_sz;
    map->ovfl = NULL;
    map->ovfl_sz = 0;
    return (map);
}

#ifdef USE_REHASH

/*
 * A hashmap does not hold the strings themselves, only index numbers
 * that refer to entries in a dict_t which holds all the data.
 * So, it is relatively easy to free up a hash table.  We do not have
 * to chase down pointers in complex data structures.
 *
 * The hashmap_t itself is relatively small, and it could be allocate
 * on the heap or it could be on the stack.  So, the caller is responsible
 * for freeing (or not) the top-level hashmap_t.
 *
 */

static void
hashmap_delete(hashmap_t *map)
{
    if (map->tbl) {
        free(map->tbl);
    }
    if (map->ovfl) {
        free(map->ovfl);
    }
}

#endif /* USE_REHASH */

/*
 * Jenkins one-at-a-time hash
 */
static size_t
hash_symbol(const char *s)
{
    uint32_t hash, i;

    for(hash = i = 0; s[i]; ++i) {
        hash += s[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

static size_t
append_ovfl(hashmap_t *map, size_t symnr)
{
    size_t ovfl_entnr;

    if (map->ovfl == NULL) {
        size_t sz = OVFL_SEGMENT_SIZE * sizeof (ovfl_t);
        map->ovfl = (ovfl_t *) guard_malloc(sz);
        map->ovfl_sz = OVFL_SEGMENT_SIZE;
        map->ovfl[0].ov_symnr = undef_symnr;
        map->ovfl[0].ov_next  = undef_ovflnr;
        map->ovfl_len = 1;
    }
    else if (map->ovfl_len >= map->ovfl_sz) {
        size_t sz = (map->ovfl_sz + OVFL_SEGMENT_SIZE) * sizeof (ovfl_t);
        map->ovfl = (ovfl_t *) guard_realloc(map->ovfl, sz);
        map->ovfl_sz += OVFL_SEGMENT_SIZE;
    }

    ovfl_entnr = map->ovfl_len;
    map->ovfl[ovfl_entnr].ov_symnr = symnr;
    map->ovfl[ovfl_entnr].ov_next  = undef_ovflnr;
    ++map->ovfl_len;
    return (ovfl_entnr);
}

static void
dict_symnr_add_hash(dict_t *dict, size_t symnr)
{
    hashmap_t *map = (hashmap_t *)dict->hashtable;
    if (map == NULL) {
        fprintf(stderr, "null hashmap\n");
        abort();
    }

    char *sym = dict_getname_nr(dict, symnr);
    size_t h = hash_symbol(sym);
    size_t bktnr = h % map->tbl_sz;
    hashbkt_t *bkt = map->tbl + bktnr;
    hashent_t *entv = bkt->ent;
    size_t entnr;
    for (entnr = 0; entnr < HASHMAP_SET_ASSOCIATIVITY; ++entnr) {
        if (entv[entnr].symnr == undef_symnr) {
            entv[entnr].h = h;
            entv[entnr].symnr = symnr;
            return;
        }
        if (entv[entnr].symnr == symnr) {
            return;
        }
    }

    size_t ovfl_entnr;

    if (bkt->chain == undef_ovflnr) {
        ovfl_entnr = append_ovfl(map, symnr);
        bkt->chain = ovfl_entnr;
        return;
    }

    ovfl_entnr = bkt->chain;
    if (map->ovfl[ovfl_entnr].ov_symnr == symnr) {
        return;
    }
    while (map->ovfl[ovfl_entnr].ov_next != undef_ovflnr) {
        ovfl_entnr = map->ovfl[ovfl_entnr].ov_next;
        if (map->ovfl[ovfl_entnr].ov_symnr == symnr) {
            return;
        }
    }

    size_t new_ovfl_entnr = append_ovfl(map, symnr);
    map->ovfl[ovfl_entnr].ov_next = new_ovfl_entnr;
    return;
}

#ifdef USE_REHASH

static void
dict_rehash_all(dict_t *dict, size_t sz)
{
    hashmap_t *oldmap;
    size_t bktnr;

    oldmap = (hashmap_t *)dict->hashtable;
    dict->hashtable = hashmap_new(sz);

    if (oldmap == NULL) {
        return;
    }

    for (bktnr = 0; bktnr <= oldmap->tbl_sz; ++bktnr) {
        hashbkt_t *bkt = oldmap->tbl + bktnr;
        hashent_t *entv = bkt->ent;
        size_t entnr;
        for (entnr = 0; entnr < HASHMAP_SET_ASSOCIATIVITY; ++entnr) {
            dict_symnr_add_hash(dict, entv[entnr].symnr);
        }
    }

    hashmap_delete(oldmap);
}

#endif /* USE_REHASH */

static size_t
dict_find_hash(dict_t *dict, const char *s, bool upsert)
{
    size_t symnr;
    size_t new_symnr;
    char *sym;
    hashmap_t *map = (hashmap_t *)dict->hashtable;
    if (map == NULL) {
        map = hashmap_new(0);
        dict->hashtable = map;
    }
    size_t h = hash_symbol(s);
    size_t bktnr = h % map->tbl_sz;
    hashbkt_t *bkt = map->tbl + bktnr;
    hashent_t *entv = bkt->ent;
    size_t entnr;
    for (entnr = 0; entnr < HASHMAP_SET_ASSOCIATIVITY; ++entnr) {
        symnr = entv[entnr].symnr;
        if (symnr == undef_symnr) {
            if (upsert) {
                new_symnr = dict_append_symbol(dict, s);
                entv[entnr].h = h;
                entv[entnr].symnr = new_symnr;
                break;
            }
            else {
                return (undef_symnr);
            }
        }

        if (h == entv[entnr].h) {
            sym = dict_getname_nr(dict, symnr);
            if (sym != NULL && strcmp(s, sym) == 0) {
                return (entv[entnr].symnr);
            }
        }
    }

    // All entries in the hash bucket itself have been searched
    // and the given symbol was not found.
    //
    // Start following the chain of overflow entries, if any.

    if (bkt->chain == undef_ovflnr) {
        // There are no overflows in this hash bucket.
        //
        if (upsert) {
            // Create an overflow entry
            // and initialize the chain for this hash bucket.
            size_t new_ovfl_entnr = append_ovfl(map, new_symnr);
            bkt->chain = new_ovfl_entnr;
            return (new_symnr);
        }
        else {
            return (undef_symnr);
        }
    }

    // There is already a chain of overflow symbols for this hash bucket.
    // Following the next links from one overflow entry to the next.
    //
    size_t ovfl_entnr;

    ovfl_entnr = bkt->chain;
    symnr = map->ovfl[ovfl_entnr].ov_symnr;
    sym = dict_getname_nr(dict, symnr);
    if (sym != NULL && strcmp(s, sym) == 0) {
        return (symnr);
    }

    while (map->ovfl[ovfl_entnr].ov_next != undef_ovflnr) {
        ovfl_entnr = map->ovfl[ovfl_entnr].ov_next;
        symnr = map->ovfl[ovfl_entnr].ov_symnr;
        sym = dict_getname_nr(dict, symnr);
        if (sym != NULL && strcmp(s, sym) == 0) {
            return (symnr);
        }
    }

    // We got to the end of the overfow chain, and found no match.
    //
    if (upsert) {
        size_t new_ovfl_entnr = append_ovfl(map, new_symnr);
        map->ovfl[ovfl_entnr].ov_symnr = new_ovfl_entnr;
        return (new_symnr);
    }

    return (undef_symnr);
}


void
dump_symbols(dict_t *dict)
{
    size_t symnr;

    for (symnr = 1; symnr < dict->len; ++symnr) {
        size_t sym_seg = symnr / dict_segment_size;
        size_t sym_off = symnr - (sym_seg * dict_segment_size);
        char **entv = dict->sv[sym_seg];
        fprintf(stderr, "%7zu = [%s]\n", symnr, entv[sym_off]);
    }
}

size_t
dict_find_linear(dict_t *dict, const char *s)
{
    size_t symnr;

    for (symnr = 1; symnr < dict->len; ++symnr) {
        size_t sym_seg = symnr / dict_segment_size;
        size_t sym_off = symnr - (sym_seg * dict_segment_size);
        char **entv = dict->sv[sym_seg];
        if (strcmp(s, entv[sym_off]) == 0) {
            return (symnr);
        }
    }
    return (undef_symnr);
}

static void
dump_dict_hashtable(dict_t *dict)
{
    size_t symnr;
    hashmap_t *map = (hashmap_t *)dict->hashtable;
    size_t bktnr;

    if (map == NULL) {
        fprintf(stderr, "No hash table.\n");
        return;
    }

    for (bktnr = 0; bktnr < map->tbl_sz; ++bktnr) {
        hashbkt_t *bkt = map->tbl + bktnr;
        hashent_t *entv = bkt->ent;
        size_t entnr;
        fprintf(stderr, "%7zu) ", bktnr);
        for (entnr = 0; entnr < HASHMAP_SET_ASSOCIATIVITY; ++entnr) {
            symnr = entv[entnr].symnr;
            if (symnr == undef_symnr) {
                break;
            }
            fprintf(stderr, " %7zu", symnr);
        }

        if (bkt->chain != undef_ovflnr) {
            size_t ovfl_entnr;

            ovfl_entnr = bkt->chain;
            fprintf(stderr, "  => ");
            fprintf(stderr, "[%zu]=%zu",
                    ovfl_entnr, map->ovfl[ovfl_entnr].ov_symnr);
            while (map->ovfl[ovfl_entnr].ov_next != undef_ovflnr) {
                ovfl_entnr = map->ovfl[ovfl_entnr].ov_next;
                fprintf(stderr, "->[%zu]=%zu",
                        ovfl_entnr, map->ovfl[ovfl_entnr].ov_symnr);
            }
        }
        fprintf(stderr, "\n");
    }
}

size_t
dict_find(dict_t *dict, const char *s)
{
    (void) upsert_true;

    if (config_use_hashtable) {
        if (dict->hashtable == NULL) {
            dict->hashtable = (hashmap_t *) hashmap_new(0);
        }
        size_t hsymnr = dict_find_hash(dict, s, upsert_false);
        size_t lsymnr = dict_find_linear(dict, s);
        if (hsymnr != lsymnr) {
            fprintf(stderr, "Error: hsymnr=%zu, lsymnr=%zu, s=[%s]\n",
                hsymnr, lsymnr, s);
            dump_dict_hashtable(dict);
            fprintf(stderr, "\nSymbols\n-------\n");
            dump_symbols(dict);
            abort();
        }
        return (hsymnr);
        // XXX return (dict_find_hash(dict, s, upsert_false));
    }
    else {
        return (dict_find_linear(dict, s));
    }
}

char *
dict_getname_nr(dict_t *dict, size_t symnr)
{
    size_t sym_seg = symnr / dict_segment_size;
    size_t sym_off = symnr - (sym_seg * dict_segment_size);
    char **entv = dict->sv[sym_seg];

    if (symnr == undef_symnr) {
        return (NULL);
    }
    return (entv[sym_off]);
}

char *
dict_getname_str(dict_t *dict, const char *s)
{
    size_t pos = dict_find(dict, s);
    return (dict_getname_nr(dict, pos));
}


/*
 * We have already determined that the symbol, |s| is not in the dictionary.
 * It is to be added.  It is appended to the symbol table.
 * |dict_append_symbol()| does not do anything with the hash table,
 * it just adds the symnr and the string.
 */

size_t
dict_append_symbol(dict_t *dict, const char *s)
{
    size_t symnr;
    size_t sym_seg;
    size_t sym_off;
    char **entv;

    symnr = dict->len;

    if (symnr >= dict->sz) {
        dict_grow(dict);
    }

    sym_seg = symnr / dict_segment_size;
    sym_off = symnr - (sym_seg * dict_segment_size);
    entv = dict->sv[sym_seg];
    entv[sym_off] = strdup(s);
    ++dict->len;
    return (symnr);
}

size_t
dict_add(dict_t *dict, const char *s)
{
    size_t symnr;

    symnr = dict_find(dict, s);
    if (symnr == undef_symnr) {
        symnr = dict_append_symbol(dict, s);
    }

    if (config_use_hashtable) {
        dict_symnr_add_hash(dict, symnr);
    }
    return (symnr);
}

void
dict_grow(dict_t *dict)
{
    size_t nseg = dict->sz / dict_segment_size;
    dict->sv = (char ***)guard_realloc(dict->sv, (nseg + 1) * sizeof (char **));
    dict->sv[nseg] = (char **)guard_malloc(dict_segment_size * sizeof (char *));
    dict->sz += dict_segment_size;
}
