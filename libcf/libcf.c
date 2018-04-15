/*
 * Filename: src/libcf/libcf.c
 * Project: incbot  and  cf
 * Library: libcf
 * Brief: Extract code / comments / string literals from C source code
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



/*
 * libcf -- C Filter.
 *
 * Filter C source code.
 * Extract / suppress any combination of:
 *   1) comments,
 *   2) string literals (in double quotes),
 *   3) character literals (in single quotes),
 *   4) code (everything else).
 *
 *
 * Libcf maintains a state machine, not unlike many that are used
 * in commonly available utilities.  Usually the programs that use
 * them are specialized.  They are designed for some specific purpose,
 * at hand, like stripping comments.  But, libcf lets you use the
 * state machine for whatever.
 *
 * They simplifying idea is to run the state machine and deliver to
 * the caller a series of { character class, character } pairs.
 * These results can be considered to be an extended character set.
 * That is, we can think of the { class, character } pair as some
 * sort of super-Unicode 64-bit character set.  We can pretend that
 * the characters inside comments are all distinct from any characters
 * outside of comments, and similarly for the super-unicode code blocks
 * for characters inside double quotes, and for those inside single quotes,
 * and all of those would be distinct from the "C code" character set.
 *
 * So, for example, if I searched for an identifier in code, it would
 * not be confused with some similar looking string inside a comment
 * or inside quotes.  The logical extension of this would be to make
 * C keywords separate super-unicode code points, after the fashion
 * of Algol; but we do not do that here.
 *
 * When operating on the stream of super-characters returned by libcf,
 * specifying which parts are to be kept and which parts are to be
 * filtered out is not much more difficult than using "ctype" functions
 * like isalpha() or isdigit().
 *
 */

#include <stdbool.h>
    // Import constant false
    // Import constant true
#include <stdio.h>
    // Import constant EOF
    // Import fprintf()
    // Import fputs()
    // Import getchar()
    // Import var stderr
    // Import var stdout
#include <stdlib.h>
    // Import abort()
    // Import exit()

#include <errno.h>
    // Import constant ENOTRECOVERABLE

#include <cf.h>
#include <cf-impl.h>

extern void *guard_malloc(size_t sz);



/*
 * The state machine cannot return character-by-character synchronous
 * results.  One character of lookahead is necessary.  For example,
 * when the state machine receives a slash, '/', it must look ahead
 * one character.  It my be the start of either style of C comment,
 * or it may just be a divide operator.
 *
 * Because there is any possibility of lookahead, the form of output
 * is an array of super-characters.  When a single character is sent
 * to the state machine, the result could be 0, 1, or 2 super-characters
 * in the result array.  When the results are delivered, the caller
 * can do pretty much whatever they want.
 *
 */


/*
 *
 * Below are the functions to manage character-class vectors.
 *
 * It has the usual suspects for poor-man's classes in C:
 *     *_new(), *_init(), *-delete().
 *
 * They array is treated like a stack.
 * super-characters are pushed.
 *
 * So, we have *_push(), *_top(), *_pop(), and *_empty() functions.
 *
 */

#define NLOOKAHEAD 4
#define CCV_SIZE(n) (sizeof (ccv_t) + (n) * sizeof (ccl_t))

void
ccv_init(ccv_t *ccv)
{
    ccv->sz  = NLOOKAHEAD;
    ccv->len = 0;
}

ccv_t *
ccv_new(void)
{
    ccv_t *ccv = guard_malloc(CCV_SIZE(NLOOKAHEAD));
    ccv_init(ccv);
    return (ccv);
}

void
ccv_delete(ccv_t *ccv)
{
    free(ccv);
}

int
ccv_push(ccv_t *ccv, int chr, int cclass)
{
    if (ccv->len >= ccv->sz) {
        return (ENOSPC);
    }

    ccv->array[ccv->len].chr = chr;
    ccv->array[ccv->len].ccl = cclass;
    ++ccv->len;
    return (0);
}

int
ccv_top(ccv_t *ccv, int *rchr, int *rccl)
{
    size_t top;

    if (ccv->len == 0) {
        *rchr = 0;
        *rccl = 0;
        return (ENODATA);
    }
    top = ccv->len - 1;
    *rchr = ccv->array[top].chr;
    *rccl = ccv->array[top].ccl;
    return (0);
}

int
ccv_pop(ccv_t *ccv, int *rchr, int *rccl)
{
    size_t top;

    if (ccv->len == 0) {
        *rchr = 0;
        *rccl = 0;
        return (ENODATA);
    }
    --ccv->len;
    top = ccv->len;
    *rchr = ccv->array[top].chr;
    *rccl = ccv->array[top].ccl;
    return (0);
}

bool
ccv_empty(ccv_t *ccv)
{
    return (ccv->len == 0);
}


/*
 * The context object used to keep track of a libcf state machine
 * is an opaque data type.  You only get to see what is inside
 * if you #include <cf-impl.h>  as well as <cf.h>.  Normal users
 * should not have to do that.
 *
 * The usual way of setting up to use libcf is to create a new
 * cf_t and a ccv_t, then feed characters by calling
 * cf_next() for each character.  Then examining the array of
 * super-characters returned by cf_next().
 *
 */

static cf_t null_ctx;

void
cf_init(cf_t *ctx, int cclset)
{
    *ctx = null_ctx;
    ctx->state  = S_START;
    ctx->cclass = cclset;
}

cf_t *
cf_new(int cclset)
{
    cf_t *ctx = (cf_t *)guard_malloc(sizeof (cf_t));
    cf_init(ctx, cclset);
    return (ctx);
}

int
cf_next(cf_t *ctx, ccv_t *rccv, int chr)
{
    int next_state;
    int next_cclass;
    bool lookahead = false;

    if (ctx->state == S_EOF || chr == EOF) {
        int rv;
        int chr;
        int ccl;

        if (ccv_empty(rccv)) {
            ccl = CC_CODE;
            chr = 0;
        }
        else {
            rv = ccv_top(rccv, &chr, &ccl);
            if (rv) {
                fprintf(stderr, "Error: ccv_top() failed.\n");
                ctx->err = ENOTRECOVERABLE;
            }
        }
        if (ccl != CC_EOF) {
            ccv_push(rccv, 0, CC_EOF);
        }
        if (chr == EOF && ctx->state != S_START) {
            fprintf(stderr, "Error: EOF and state==%d (%s)\n",
                ctx->state, decode_state(ctx->state));
            ctx->err = ENOTRECOVERABLE;
        }
        ctx->state = S_EOF;
        if (ctx->err) {
            return (-ctx->err);
        }
        return (EOF);
    }

    next_state  = ctx->state;
    next_cclass = ctx->cclass;
    switch (ctx->state) {
    case S_START:
        ctx->cclass = CC_CODE;
        switch (chr) {
        case '"':
            ctx->cclass = CC_OUTER_STRING;
            next_cclass = CC_INNER_STRING;
            next_state = S_START_DQUOTE;
            break;
        case '\'':
            ctx->cclass = CC_OUTER_CHAR;
            next_cclass = CC_INNER_CHAR;
            next_state = S_START_SQUOTE;
            break;
        case '/':
            // Could be start of comment, either /*...*/ or // ... EOL
            // Or could be division
            ctx->cclass = CC_UNDEF;
            next_state = S_START_SLASH;
            lookahead = true;
            break;
        default:
            // No comment starter and no strings.
            // So, keep looping in start state.
            break;
        }
        break;

    case S_START_DQUOTE:
        switch (chr) {
        case '\\':
            // Escape character inside dquote-string
            next_state = S_DQUOTE_ESCAPE;
            break;
        case '"':
            // Close of double-quote string -- " ... "
            ctx->cclass = CC_OUTER_STRING;
            next_state = S_START;
            break;
        }
        break;

    case S_DQUOTE_ESCAPE:
        next_state = S_START_DQUOTE;
        break;

    case S_START_SQUOTE:
        // Start of squote-string, that is, a C character literal.
        switch (chr) {
        case '\\':
            next_state = S_SQUOTE_ESCAPE;
            break;
        case '\'':
            // End of squote-string
            next_state = S_START;
            if ((ctx->cclass & CC_INNER_CHAR) == 0) {
                fprintf(stderr, "INTERNAL ERROR.\n");
                ctx->err = ENOTRECOVERABLE;
                abort();
            }
            ctx->cclass = CC_OUTER_CHAR;
            break;
        }
        break;

    case S_SQUOTE_ESCAPE:
        next_state = S_START_SQUOTE;
        break;

    case S_START_SLASH:
        // Could be start of comment, either /*...*/ or // ... EOL
        switch (chr) {
        case '/':
            ctx->cclass = CC_OUTER_COMMENT;
            next_cclass = CC_INNER_COMMENT;
            next_state = S_COMMENT_EOL;
            break;
        case '*':
            ctx->cclass = CC_OUTER_COMMENT;
            next_cclass = CC_INNER_COMMENT;
            next_state = S_SLASH_STAR;
            break;
        default:
            ctx->cclass = CC_CODE;
            next_state = S_START;
        }
        ccv_push(rccv, '/', ctx->cclass);
        break;

    case S_SLASH_STAR:
        switch (chr) {
        case '*':
        // Could be end of comment of the form /* ... */
        next_state = S_SLASH_STAR_STAR;
        }
        break;

    case S_SLASH_STAR_STAR:
        // Could be end of comment of the form /* ... */
        switch (chr) {
        case '/':
            // Yes, it is the end of a comment.
            next_state = S_START;
            break;
        case '*':
            // Still can be at end of /* ... *  if we see another '*'
            // So, stay in this state, until we see the next '/'.
            break;
        default:
            // Not '/' and not '*', so go back to inside comment
            next_state = S_SLASH_STAR;
        }
        break;

    case S_COMMENT_EOL:
        switch (chr) {
        case '\n':
            // End of //-comment
            next_state = S_START;
            break;
        }
        break;
    }

    if (!lookahead) {
        ccv_push(rccv, chr, ctx->cclass);
    }

    ctx->cclass = next_cclass;
    ctx->state  = next_state;
    if (ctx->state == S_START) {
        ctx->cclass = CC_CODE;
    }

    return (0);
}
