/*
 * Filename: src/libincbot/incbot.c
 * Project: incbot
 * Library: libincbot
 * Brief: Generate C preprocessor #include directives based on identifiers
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

#include <cf.h>         // ccv_t, ccl_t, cclass_set::CC_CODE,
                        // cclass_set::CC_EOF, ccv_new, cf_new, cf_next, cf_t,
                        // cclass_set::CC_ERR
#include <cscript.h>    // guard_malloc, guard_realloc
#include <ctype.h>      // isalpha, isdigit, isspace
#include <errno.h>      // errno, ENOBUFS
#include <stdbool.h>    // bool
#include <stddef.h>     // size_t, NULL
#include <stdio.h>      // fprintf, stderr, printf, EOF, fgetc, FILE, fclose,
                        // fopen, fputc, getc, stdin
#include <stdlib.h>     // exit, qsort
#include <string.h>     // strcmp
#include "dict.h"       // dict_getname_nr, dict_add, undef_symnr, dict_new,
                        // dict_t


struct ioresult {
    int err;
    size_t sz;
};

typedef struct ioresult ioresult_t;


#include <stdarg.h>

static inline ioresult_t
fwrite_str(FILE *f, char *str)
{
    ioresult_t res;
    int rc;

    rc = fputs(str, f);
    if (rc == EOF) {
        res.err = errno;
    }
    else {
        res.err = 0;
    }
    res.sz = (size_t)rc;
    return (res);
}

static inline ioresult_t
write_str(char *str)
{
    return (fwrite_str(stdout, str));
}

static inline ioresult_t
fwrite_strl(FILE *f, ...)
{
    va_list ap;
    char *str;
    ioresult_t res;
    ioresult_t res1;

    va_start(ap, f);
    res.sz = 0;
    while ((str = va_arg(ap, char *)) != NULL) {
        res1 = fwrite_str(f, str);
        if (res1.err) {
            res.err = res1.err;
            break;
        }
        else {
            res.sz = res1.sz;
        }
    }
    va_end(ap);
    return (res);
}

void
io_guard(ioresult_t res)
{
    if (res.err) {
        eprintf("I/O error, %d.\n", res.err);
        abort();
    }
}

#if 0

static inline ioresult_t
write_strl(str1, ...)
{

}

#endif


typedef size_t index_t;

extern bool verbose;
extern bool debug;

extern FILE *errprint_fh;
extern FILE *dbgprint_fh;

enum sym_type {
    TYPE_UNKNOWN  = 0x01,
    TYPE_FUNCTION = 0x02,
    TYPE_TYPEDEF  = 0x04,
    TYPE_KEYWORD  = 0x08,
    TYPE_CONSTANT = 0x10,
    TYPE_VAR      = 0x20,
    TYPE_STRUCT   = 0x40,
};

#define TYPE_ALL (TYPE_UNKNOWN|TYPE_FUNCTION|TYPE_TYPEDEF|TYPE_KEYWORD|TYPE_CONSTANT|TYPE_VAR|TYPE_STRUCT)

struct idinfo {
    size_t sym;
    int    type;
    size_t src1;
    size_t standard1;
    size_t standard2;
    size_t man_sect;
    size_t man_path;
    size_t declare;
    bool   trace;
};

typedef struct idinfo idinfo_t;


struct incref {
    size_t ref_sym;
    size_t inc_sym;
    size_t ref_idnr;
    size_t ref_lnr;
};

typedef struct incref incref_t;

static const size_t undef_idnr = (size_t)(-1);

static dict_t *id_symtable;	// Symbol table for identifiers
static dict_t *strtable;	// SYmbol table for all other strings

static idinfo_t virgin_idinfo;
static const size_t id_table_segment_size = 64; // 8 * 1024;
static idinfo_t *id_table;
static size_t id_table_sz;
static size_t id_table_len;

static const size_t ref_inc_table_segment_size = 64; //8 * 1024;
static incref_t *ref_inc_table;
static size_t ref_inc_table_sz;
static size_t ref_inc_table_len;

static int fsep = ';';

void
init_tables(void)
{
    size_t sz;

    id_table_sz  = id_table_segment_size;
    id_table_len = 0;
    sz = id_table_sz * sizeof (idinfo_t);
    id_table = (idinfo_t *) guard_malloc(sz);

    ref_inc_table_sz  = ref_inc_table_segment_size;
    ref_inc_table_len = 0;
    sz = ref_inc_table_sz * sizeof (incref_t);
    ref_inc_table = (incref_t *) guard_malloc(sz);

    id_symtable = dict_new();
    strtable = dict_new();
}

void
id_table_grow(void)
{
    size_t sz;

    id_table_sz += id_table_segment_size;
    sz = id_table_sz * sizeof (idinfo_t);
    id_table = (idinfo_t *) guard_realloc(id_table, sz);
}

void
ref_inc_table_grow(void)
{
    size_t sz;

    ref_inc_table_sz += ref_inc_table_segment_size;
    sz = ref_inc_table_sz * sizeof (incref_t);
    ref_inc_table = (incref_t *) guard_realloc(ref_inc_table, sz);
}

static int
encode_id_type(const char *s)
{
    int t;

    t = (s[0] && !s[1]) ? s[0] : -1;
    switch (t) {
    case 'c': return (TYPE_CONSTANT);
    case 'f': return (TYPE_FUNCTION);
    case 'k': return (TYPE_KEYWORD);
    case 's': return (TYPE_STRUCT);
    case 't': return (TYPE_TYPEDEF);
    case 'v': return (TYPE_VAR);
    case '?': return (TYPE_UNKNOWN);
    }
    return (-1);
}

static char *
decode_id_type(int t)
{
    static char dcbuf[8];
    char *p;

    p = dcbuf;
    if (t == -1) {
        *p++ = '?';
        t = 0;
    }

    if (t & TYPE_CONSTANT) {
        *p++ = 'c';
    }
    if (t & TYPE_FUNCTION) {
        *p++ = 'f';
    }
    if (t & TYPE_KEYWORD) {
        *p++ = 'k';
    }
    if (t & TYPE_STRUCT) {
        *p++ = 's';
    }
    if (t & TYPE_TYPEDEF) {
        *p++ = 't';
    }
    if (t & TYPE_VAR) {
        *p++ = 'v';
    }
    if (t & TYPE_UNKNOWN) {
        *p++ = '?';
    }
    *p = '\0';
    return (dcbuf);
}

static char *
annotate_type(int t)
{
    switch (t) {
    default: return ("?");
    case TYPE_CONSTANT: return ("constant");
    case TYPE_FUNCTION: return ("function");
    case TYPE_KEYWORD:  return ("keyword");
    case TYPE_STRUCT:   return ("struct");
    case TYPE_TYPEDEF:  return ("type");
    case TYPE_VAR:      return ("var");
    case TYPE_UNKNOWN:  return ("unknown");
    }
}

static void
verify_idtable(void)
{
    index_t pos;
    size_t err_count;

    err_count = 0;
    for (pos = 0; pos < id_table_len; ++pos) {
        index_t symnr = id_table[pos].sym;
        if (symnr != pos + 1) {
            eprintf("id_table[pos==%zu].sym == %zu.\n", pos, symnr);
            ++err_count;
            if (err_count >= 10) {
                break;
            }
        }
    }

    if (err_count) {
        eprintf("The following assumption does not hold:\n");
        eprintf("    id_table is in sync with id_symtable.\n");
        exit(2);
    }
}

// XXX OBSOLETE

size_t
id_find_linear_search(const char *s, int type_mask)
{
    size_t pos;

    for (pos = 0; pos < id_table_len; ++pos) {
        size_t symnr = id_table[pos].sym;
        const char *sym = dict_getname_nr(id_symtable, symnr);
        int types = id_table[pos].type;

        if (sym != NULL && strcmp(s, sym) == 0 && (types & type_mask) != 0) {
            return (pos);
        }
    }
    return (undef_idnr);
}

static void
fshow_typemask(FILE *f, int type_mask)
{
    size_t tm;
    size_t m;
    size_t mcnt;

    if (type_mask == 0) {
        return;
    }

    tm = (size_t)type_mask;

    mcnt = 0;
    for (m = ~((size_t)-1 & ((size_t)-1 >> 1)) ; m != 0; m >>= 1) {
        if ((tm & m) != 0) {
            if (mcnt != 0) {
                fputc('|', f);
            }
            else {
                fputc('{', f);
            }
            // fprintf(f, ",m=%zu", m);
            fputs(annotate_type((int)m), f);
            ++mcnt;
        }
    }

    if (mcnt != 0) {
        fputc('}', f);
    }
}

static index_t
id_find(const char *s, int type_mask)
{
    index_t symnr;
    index_t id_pos;
    int t;

    symnr = dict_find(id_symtable, s);
    if (symnr == undef_symnr) {
        return (undef_idnr);
    }

    id_pos = symnr - 1;
    t = id_table[id_pos].type;

    if (id_table[id_pos].trace) {
        fprintf(stderr, "id_find:\n  id=[%s]\n  type_mask=%x=", s, type_mask);
        fshow_typemask(stderr, type_mask);
        fprintf(stderr, "\n  type(%s)=%x=%s\n", s, t, annotate_type(t));
    }

    if ((t & type_mask) == 0) {
        return (undef_idnr);
    }

    return (id_pos);
}

int
add_id_field(const char *fld_str, size_t fidx)
{
    idinfo_t *id_ent;

    if (id_table_len >= id_table_sz) {
        id_table_grow();
    }
    id_ent = id_table + id_table_len;

    dbg_printf("%s: @%zu: field[%zu] = '%s'\n",
        __FUNCTION__, id_table_len, fidx, fld_str);

    switch (fidx) {
    case 0:
        *id_ent = virgin_idinfo;
        id_ent->type = encode_id_type(fld_str);
        break;
    case 1:
        id_ent->man_sect = dict_add(strtable, fld_str);
        break;
    case 2:
        id_ent->sym = dict_add(id_symtable, fld_str);
        break;
    case 3:
        id_ent->src1 = dict_add(strtable, fld_str);
        break;
    case 4:
        id_ent->standard1 = dict_add(strtable, fld_str);
        break;
    case 5:
        id_ent->standard2 = dict_add(strtable, fld_str);
        break;
    case 6:
        id_ent->man_path = dict_add(strtable, fld_str);
        break;
    case 7:
        if (fld_str && fld_str[0]) {
            id_ent->declare = dict_add(strtable, fld_str);
        }
        break;
    }
    return (0);
}


static void
add_ref_inc_pair(size_t inc_symnr, size_t ref_symnr, size_t id_ent, size_t lnr)
{
    if (ref_inc_table_len >= ref_inc_table_sz) {
        ref_inc_table_grow();
    }
    ref_inc_table[ref_inc_table_len].ref_sym  = ref_symnr;
    ref_inc_table[ref_inc_table_len].inc_sym  = inc_symnr;
    ref_inc_table[ref_inc_table_len].ref_idnr = id_ent;
    ref_inc_table[ref_inc_table_len].ref_lnr  = lnr;
    ++ref_inc_table_len;
}

#define ERR_LIMIT 10
#define NFLD 8

int
read_id_table_stream(FILE *f, const char *fname)
{
    char fbuf[1024];
    size_t fbuf_sz;
    size_t fbuf_len;
    size_t lnr;
    size_t fidx;
    size_t lcol;
    size_t err_count;
    int c;

    if (id_table_sz == 0) {
        init_tables();
    }

    fbuf_sz  = sizeof (fbuf);
    fbuf_len = 0;
    lnr = 0;
    fidx = 0;
    lcol = 0;
    err_count = 0;
    while ((c = fgetc(f)) != EOF) {
        if (c == '#' && lcol == 0) {
            while ((c = fgetc(f)) != EOF && c != '\n') {
                // skip
            }
        }

        if ((c == '\n' && lcol != 0) || c == fsep) {
            fbuf[fbuf_len] = '\0';
            add_id_field(fbuf, fidx);
            fbuf_len = 0;
            if (c == '\n') {
                if (fidx >= NFLD) {
                    eprintf("More than %u fields per line.\n", NFLD);
                    eprintf("Extra fields will be ignored.\n");
                    eprintf("File: %s\n", fname);
                    eprintf("Line %zu.\n", lnr);
                    ++err_count;
                    if (err_count > ERR_LIMIT) {
                        eprintf("Too many errors.  Bailing out.\n");
                        return (2);
                    }
                }

                if (id_table_len >= id_table_sz) {
                    id_table_grow();
                }
                ++id_table_len;
            }
            else {
                ++fidx;
            }
        }
        else if (c != EOF && c != '\n') {
            if (fbuf_len < fbuf_sz) {
                fbuf[fbuf_len++] = c;
            }
            else {
                eprintf("field too long.\n");
                eprintf("File: %s\n", fname);
                eprintf("Line %zu.\n", lnr);
                while ((c = getc(f)) != EOF && c != fsep && c != '\n') {
                    // Skip
                }
            }
        }

        if (c == '\n') {
            ++lnr;
            fidx = 0;
            lcol = 0;
        }
        else {
            ++lcol;
        }
    }

    return (err_count ? 1 : 0);
}

static const char *endl = "\n";
static const int SQ = '\'';

static inline char *
vischar_r(char *buf, size_t sz, int c)
{
    if (isprint(c)) {
        buf[0] = c;
        buf[1] = '\0';
    }
    else {
        snprintf(buf, sz, "\\x%02x", c);
    }
    return (buf);
}

static inline void
ieputs(const char *str)
{
    fputs(str, stderr);
}

static inline void
ieputc(int chr)
{
    fputc(chr, stderr);
}

static void
fshow_path(FILE *f, const char *path)
{
    const char *s;
    char visbuf[4];

    for (s = path; *s; ++s) {
        if (isprint(*s)) {
            fputc(*s, f);
        }
        else {
            fputs(vischar_r(visbuf, sizeof (visbuf), *s), f);
        }
    }
}

static inline void
fshow_quoted_path(FILE *f, const char *path)
{
    fputc(SQ, f);
    fshow_path(f, path);
    fputc(SQ, f);
}


static inline void
ieshow_path(const char *path)
{
    fshow_path(stderr, path);
}

static inline void
ieshow_quoted_path(const char *path)
{
    fshow_quoted_path(stderr, path);
}

int
read_id_table_file(const char *fname)
{
    FILE *srcf;
    int rv;

    if (verbose) {
        ieputs("id_table=");
        ieshow_quoted_path(fname);
        ieputs(endl);
    }

    srcf = fopen(fname, "r");
    if (srcf == NULL) {
        int err;

        err = errno;
        eprintf("open('%s', r) failed.\n", fname);
        return (err);
    }

    rv = read_id_table_stream(srcf, fname);
    fclose(srcf);

    verify_idtable();

    return (rv);
}

static inline bool
is_identifier_start(int chr)
{
    return (isalpha(chr) || chr == '_');
}

static inline bool
is_identifier(int chr)
{
    return (isalpha(chr) || isdigit(chr) || chr == '_');
}

size_t necho = 0;
size_t count_getc = 0;

int
cf_getc(FILE *f, cf_t *cf, ccv_t *ccv)
{
    size_t len;
    size_t pos;
    int rv;

    while (true) {
        while (ccv->len == 0) {
            int c;

            c = fgetc(f);
            if (necho) {
                ++count_getc;
                if (count_getc < necho) {
                    fputc(c, errprint_fh);
                }
                else if (count_getc == necho) {
                    fputc('\n', errprint_fh);
                }
            }

            rv = cf_next(cf, ccv, c);
            if (rv != 0 && rv != EOF) {
                // XXX Maybe libcf should push { CC_ERR, errno } onto ccv
                eprintf("cf_next() failed; err=%d\n", rv);
                exit(rv);
            }
        }

        len = ccv->len;
        for (pos = 0; pos < len; ++pos) {
            int ccl = ccv->array[pos].ccl;
            int chr = ccv->array[pos].chr;

            if (ccl == CC_EOF) {
                chr = EOF;
            }

            if (ccl == CC_ERR || ccl == CC_EOF || (ccl & CC_CODE) != 0) {
                size_t dst;

                ++pos;
                for (dst = 0; pos < len; ++pos) {
                    ccv->array[dst] = ccv->array[pos];
                    ++dst;
                }
                ccv->len = dst;
                return (chr);
            }
        }

        ccv->len = 0;
    }
}

static void
cf_ungetc(int c, ccv_t *ccv)
{
    size_t sz;
    size_t len;
    size_t pos;

    sz  = ccv->sz;
    len = ccv->len;
    if (len >= sz) {
        eprintf("Out of space in cf character class buffer.\n");
        eprintf("Capacity is %zu, which ought to be enough.\n", sz);
        exit(ENOBUFS);
    }

    for (pos = len; pos != 0; --pos) {
        ccv->array[pos] = ccv->array[pos - 1];
    }
    ++ccv->len;
    ccv->array[0].chr = c;
    ccv->array[0].ccl = CC_CODE;
}

static int
incbot_src_stream(FILE *f, const char *fname)
{
    ccv_t *ccv = ccv_new();
    cf_t *cf = cf_new(CC_CODE);
    size_t lnr;
    size_t col;
    char idbuf[1024];
    int c;
    bool in_preprocessor;

    lnr = 0;
    col = 0;
    in_preprocessor = false;
    while ((c = cf_getc(f, cf, ccv)) != EOF) {
        if (c == '\n') {
            ++lnr;
            col = 0;
            in_preprocessor = false;
        }
        else {
            ++col;
        }

        if (col == 1 && c == '#') {
            in_preprocessor = true;
        }

        if (is_identifier_start(c)) {
            size_t idnr;
            char *dstp;
            int find_type = TYPE_ALL & ~TYPE_FUNCTION;

            dstp = idbuf;
            *dstp++ = c;
            while ((c = cf_getc(f, cf, ccv)) != EOF && is_identifier(c)) {
                *dstp++ = c;
                ++col;
            }
            *dstp = '\0';
            while (c != EOF && isspace(c)) {
                c = cf_getc(f, cf, ccv);
                ++col;
            }
            if (c == '(') {
                find_type = TYPE_FUNCTION | TYPE_KEYWORD;
            }
            else if (is_identifier_start(c)) {
                cf_ungetc(c, ccv);
            }

            // Do not look at the contents of an #incude directive
            // A filename can be mistaken for an identifier
            // @library{libcf} only handles suppression of comments,
            // and string literals, and knows nothing about
            // preprocessor directives.
            //
            if (in_preprocessor && strcmp(idbuf, "include") == 0) {
                while ((c = cf_getc(f, cf, ccv)) != EOF && c != '\n') {
                    ///
                }

                if (c == '\n') {
                    ++lnr;
                    col = 0;
                    in_preprocessor = false;
                }
                else if (c == EOF) {
                    break;
                }
                else {
                    ++col;
                }
            }

            idnr = id_find(idbuf, find_type);
            if (idnr != undef_idnr) {
                idinfo_t *id_ent;
                size_t ref_symnr;
                size_t inc_symnr;
                char *ref_sym;
                char *inc_sym;

                id_ent = id_table + idnr;
                ref_symnr = id_ent->sym;
                ref_sym = dict_getname_nr(id_symtable, ref_symnr);
                inc_symnr = id_ent->src1;
                if (inc_symnr != undef_symnr) {
                    inc_sym = dict_getname_nr(strtable, inc_symnr);
                    if (inc_sym && *inc_sym && id_ent->type != TYPE_KEYWORD) {
                        add_ref_inc_pair(inc_symnr, ref_symnr, idnr, lnr);
                    }
                }

                dbg_printf("File: %s\n", fname);
                dbg_printf("lnr=%zu, id=%zu=[%s], type=%s",
                    lnr, ref_symnr, ref_sym, decode_id_type(id_ent->type));

                if (id_ent->type == TYPE_TYPEDEF && id_ent->declare) {
                    size_t decl_symnr;
                    char *decl_sym;

                    decl_symnr = id_ent->declare;
                    decl_sym = dict_getname_nr(id_symtable, decl_symnr);
                    dbg_printf(", %s", decl_sym);
                }
                dbg_printf("\n");
            }
#ifdef CSCRIPT_DEBUG
            else {
                dbg_printf("lnr=%zu, id=[%s] => NULL\n", lnr, idbuf);
            }
#endif
            if (c == '\n') {
                ++lnr;
            }
            if (c == EOF) {
                break;
            }
        }
    }

    return (0);
}

int
incbot_src_file(const char *fname)
{
    FILE *f;
    int err;

    if (fname[0] == '-' && !fname[1]) {
        f = stdin;
    }
    else {
        f = fopen(fname, "r");
        if (f == NULL) {
            err = errno;
            eprintf("open('%s', r) failed.\n", fname);
            return (err);
        }
    }

    err = incbot_src_stream(f, fname);

    fclose(f);
    return (err);
}

int
refcmp(const void *vref1, const void *vref2)
{
    const incref_t *ref1 = (const incref_t *)vref1;
    const incref_t *ref2 = (const incref_t *)vref2;
    const char *inc_fname1 = dict_getname_nr(strtable, ref1->inc_sym);
    const char *inc_fname2 = dict_getname_nr(strtable, ref2->inc_sym);
    int cmp = strcmp(inc_fname1, inc_fname2);
    if (cmp == 0) {
        const char *ref_sym1 = dict_getname_nr(id_symtable, ref1->ref_sym);
        const char *ref_sym2 = dict_getname_nr(id_symtable, ref2->ref_sym);
        cmp = strcmp(ref_sym1, ref_sym2);
    }
    return (cmp);
}


void
show_includes(void)
{
    size_t refnr;
    size_t prev_inc_symnr = undef_symnr;
    size_t prev_ref_symnr = undef_symnr;

    qsort((void *)ref_inc_table, ref_inc_table_len, sizeof (incref_t), refcmp);
    for (refnr = 0; refnr < ref_inc_table_len; ++refnr) {
        size_t inc_symnr = ref_inc_table[refnr].inc_sym;
        size_t ref_symnr = ref_inc_table[refnr].ref_sym;
        size_t id_ent    = ref_inc_table[refnr].ref_idnr;
        // size_t lnr    = ref_inc_table[refnr].ref_lnr;
        if (refnr == 0 || inc_symnr != prev_inc_symnr) {
            char *inc_sym = dict_getname_nr(strtable, inc_symnr);
            char *p1;
            char *p2;

            p1 = inc_sym;
            p2 = p1;
            while (true) {
                if (*p2 == '\0' || *p2 == '|') {
                    size_t len = p2 - p1;
                    if (len) {
                        fputs("#include <", stdout);
                        fwrite(p1, len, 1, stdout);
                        fputs(">\n", stdout);
                    }
                    if (*p2 == '\0') {
                        break;
                    }
                    p1 = p2 + 1;
                    p2 = p1;
                }
                ++p2;
            }
            
            // printf("#include <%s>\n", inc_sym);
            prev_inc_symnr = inc_symnr;
            prev_ref_symnr = undef_symnr;
        }

        if (ref_symnr != prev_ref_symnr) {
            char *ref_sym = dict_getname_nr(id_symtable, ref_symnr);
            int id_types = id_table[id_ent].type;
            io_guard(write_str("    // Import "));
            // printf("    // Import ");
            if (id_types & TYPE_FUNCTION) {
                printf("%s()", ref_sym);
            }
            else if (id_types & TYPE_UNKNOWN) {
                io_guard(write_str(ref_sym));
                // printf("%s", ref_sym);
            }
            else {
                printf("%s %s", annotate_type(id_types), ref_sym);
            }
            printf("\n");
            prev_ref_symnr = ref_symnr;
        }
    }
}

int
read_config_file(const char *path)
{
    eprintf("read_config_file(%s):\n", path);
    eprintf("*** not implemented ***\n");
    exit(2);
}

int
trace_identifier(const char *sym)
{
    index_t idnr;

    idnr = id_find(sym, TYPE_ALL);
    if (idnr == undef_idnr) {
        return (ENOENT);
    }
    id_table[idnr].trace = true;
    return (0);
}
