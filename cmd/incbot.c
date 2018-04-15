/*
 * Filename: src/cmd/incbot.c
 * Project: incbot
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

// IWYU::START
#include <ctype.h>      // isprint
#include <getopt.h>     // no_argument, getopt_long, required_argument, option
#include <incbot.h>     // read_id_table_file, incbot_src_file,
                        // read_config_file, show_includes
#include <stdbool.h>    // bool, true, false
#include <stddef.h>     // size_t, NULL
#include <stdio.h>      // fputs, fputc, FILE, snprintf, stdout
#include <stdlib.h>     // exit
#include "cscript.h"    // eprintf, filev_probe, eprint, fshow_str_array,
                        // set_debug_fh, set_eprint_fh, sname
#include <string.h>     // memcpy()
// IWYU::END

#include <incbot.h>

const char *program_path;
const char *program_name;
char *program_realpath;

size_t filec;               // Count of elements in filev
char **filev;               // Non-option elements of argv

bool verbose = false;
bool debug   = false;

FILE *errprint_fh = NULL;
FILE *dbgprint_fh = NULL;

static const char *default_id_table
    = "/home/shaw/v/psdk/dist/share/lib/incbot/id-table";
//  = "/usr/local/           /share/lib/incbot/id-table";

static struct option long_options[] = {
    {"help",           no_argument,       0,  'h'},
    {"version",        no_argument,       0,  'V'},
    {"verbose",        no_argument,       0,  'v'},
    {"debug",          no_argument,       0,  'd'},
    {"conf",           required_argument, 0,  'c'},
    {"id-table",       required_argument, 0,  't'},
    {"trace",          required_argument, 0,  'T'},
    {0, 0, 0, 0}
};

static const char usage_text[] =
    "Options:\n"
    "  --help|-h|-?         Show this help message and exit\n"
    "  --version            Show version information and exit\n"
    "  --verbose|-v         verbose\n"
    "  --debug|-d           debug\n"
    "  --conf      <fname>  configuration file\n"
    "  --id-table  <fname>  load file containing descriptions of identifiers\n"
    "                       There can be any number of id-table files.\n"
    "  --trace=<symbol>     Trace usage of the given symbol\n"
    "                       There can be any number of --trace=symbol\n"
    ;

static const char version_text[] =
    "0.1\n"
    ;

static const char copyright_text[] =
    "Copyright (C) 2016 Guy Shaw\n"
    "Written by Guy Shaw\n"
    ;

static const char license_text[] =
    "License GPLv3+: GNU GPL version 3 or later"
    " <http://gnu.org/licenses/gpl.html>.\n"
    "This is free software: you are free to change and redistribute it.\n"
    "There is NO WARRANTY, to the extent permitted by law.\n"
    ;

static void
fshow_program_version(FILE *f)
{
    fputs(version_text, f);
    fputc('\n', f);
    fputs(copyright_text, f);
    fputc('\n', f);
    fputs(license_text, f);
    fputc('\n', f);
}

static void
show_program_version(void)
{
    fshow_program_version(stdout);
}

static void
usage(void)
{
    eprintf("usage: %s [ <options> ]\n", program_name);
    eprint(usage_text);
}

static inline bool
is_long_option(const char *s)
{
    return (s[0] == '-' && s[1] == '-');
}

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

int
incbot_all_files(size_t filec, char **filev)
{
    size_t fnr;
    int err;

    err = filev_probe(filec, filev);
    if (err) {
        return (err);
    }

    for (fnr = 0; fnr < filec; ++fnr) {
        err = incbot_src_file(filev[fnr]);
        if (err) {
            return (err);
        }
        show_includes();
    }

    return (err);
}


// ========== Section: manage tracing of selected identifiers ==========

static const char *trcv[64];
static size_t ntrace;

static void
add_trace_identifiers(const char *id)
{
    trcv[ntrace] = id;
    ++ntrace;
}

static void
mark_all_traced_identifiers(void)
{
    size_t trcnr;

    for (trcnr = 0; trcnr < ntrace; ++trcnr) {
        trace_identifier(trcv[trcnr]);
    }
}

// ==========


int
main(int argc, char **argv)
{
    extern char *optarg;
    extern int optind, opterr, optopt;
    int option_index;
    int err_count;
    int optc;
    int rv;
    bool have_id_table = false;

    ntrace = 0;
    set_eprint_fh();
    program_path = *argv;
    program_name = sname(program_path);
    option_index = 0;
    err_count = 0;
    opterr = 0;

    while (true) {
        int this_option_optind;

        if (err_count > 10) {
            eprintf("%s: Too many option errors.\n", program_name);
            break;
        }

        this_option_optind = optind ? optind : 1;

        optc = getopt_long(argc, argv, "+hVdvc:t:T:", long_options, &option_index);
        if (optc == -1) {
            break;
        }

        rv = 0;
        if (optc == '?' && optopt == '?') {
            optc = 'h';
        }

        switch (optc) {
        case 'V':
            show_program_version();
            exit(0);
            break;
        case 'h':
            fputs(usage_text, stdout);
            exit(0);
            break;
        case 'd':
            debug = true;
            set_debug_fh(NULL);
            break;
        case 'v':
            verbose = true;
            break;
        case 'c':
            rv = read_config_file(optarg);
            break;
        case 't':
            rv = read_id_table_file(optarg);
            //  Here, it matters only that a table was mentioned,
            //  not whether it was read without errors.
            have_id_table = true;
            break;
        case 'T':
            add_trace_identifiers(optarg);
            break;
        case '?':
            eprint(program_name);
            eprint(": ");
            if (is_long_option(argv[this_option_optind])) {
                eprintf("unknown long option, '%s'\n",
                    argv[this_option_optind]);
            }
            else {
                char chrbuf[10];
                eprintf("unknown short option, '%s'\n",
                    vischar_r(chrbuf, sizeof (chrbuf), optopt));
            }
            ++err_count;
            break;
        default:
            eprintf("%s: INTERNAL ERROR: unknown option, '%c'\n",
                program_name, optopt);
            exit(64);
            break;
        }
    }

    verbose = verbose || debug;

    if (argc != 0) {
        filec = (size_t) (argc - optind);
        filev = argv + optind;
    }
    else {
        filec = 0;
        filev = NULL;
    }

    if (verbose) {
        fshow_str_array(errprint_fh, filec, filev);
    }

    if (verbose && optind < argc) {
        eprintf("non-option ARGV-elements:\n");
        while (optind < argc) {
            eprintf("    %s\n", argv[optind]);
            ++optind;
        }
    }

    if (err_count != 0) {
        usage();
        exit(1);
    }

    if (!have_id_table) {
        rv = access(default_id_table, R_OK);
        if (rv == 0) {
            rv = read_id_table_file(default_id_table);
            if (rv != 0) {
                exit(rv);
            }
            have_id_table = true;
        }
    }

    if (!have_id_table) {
        program_realpath = realpath(program_path, NULL);
        const char *cmd_sfx = "/src/cmd/incbot";
        const char *tbl_sfx = "/src/table/id-table";
        size_t len = strlen(program_realpath);
        size_t off = len - strlen(cmd_sfx);

        // fprintf(stderr, "program_realpath = [%s]\n", program_realpath);
        // fprintf(stderr, "  + off=%zu = [%s]\n", off, program_realpath + off);

        if (strcmp(program_realpath + off, cmd_sfx) == 0) {
            char *tbl_path = guard_malloc(off + strlen(tbl_sfx) + 1);
            memcpy(tbl_path, program_realpath, off);
            memcpy(tbl_path + off, tbl_sfx, strlen(tbl_sfx) + 1);
            // fprintf(stderr, "tbl_path=[%s]\n", tbl_path);
            rv = access(tbl_path, R_OK);
            if (rv == 0) {
                rv = read_id_table_file(tbl_path);
                if (rv != 0) {
                    exit(rv);
                }
                have_id_table = true;
            }
        }
    }

    if (!have_id_table) {
        eprintf("Could not find identifier table.\n");
        exit(2);
    }

    mark_all_traced_identifiers();
    if (filec == 0) {
        char *filev_stdin[] = { "-" };
        rv = incbot_all_files(1, filev_stdin);
    }
    else {
        rv = filev_probe(filec, filev);
        if (rv != 0) {
            exit(rv);
        }

        rv = incbot_all_files(filec, filev);
    }

    if (rv != 0) {
        exit(rv);
    }

    exit(0);
}
