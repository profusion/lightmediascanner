/**
 * Copyright (C) 2008-2011 by ProFUSION embedded systems
 * Copyright (C) 2007 by INdT
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * @author Gustavo Sverzut Barbieri <barbieri@profusion.mobi>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <lightmediascanner.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <sqlite3.h>
#include <errno.h>

static int color = 0;
static const char short_options[] = "s:S:p:P::c:i:t:m:v::h";

static const struct option long_options[] = {
    {"scan-path", 1, NULL, 's'},
    {"show-path", 1, NULL, 'S'},
    {"parser", 1, NULL, 'p'},
    {"list-parsers", 2, NULL, 'P'},
    {"charset", 1, NULL, 'c'},
    {"commit-interval", 1, NULL, 'i'},
    {"slave-timeout", 1, NULL, 't'},
    {"method", 1, NULL, 'm'},
    {"verbose", 2, NULL, 'v'},
    {"help", 0, NULL, 'h'},
    {NULL, 0, 0, 0}
};

static const char *help_texts[] = {
    "Path to scan for new media",
    "Path to show known media",
    "Parser path or name to add",
    "List all know parsers, with argument list of that category",
    "Charset to add",
    "Commit interval, in number of transactions",
    "Slave timeout, in milliseconds",
    "Work method to use: 'dual' for two process (safe) or 'mono' for one.",
    "verbose mode, print progress (=0 to disable it)",
    "this help message",
    NULL
};

static void
show_help(const char *prg_name)
{
    const struct option *lo;
    const char **help;
    int largest;

    fprintf(stderr,
            "Usage:\n"
            "\t%s [options] <database-path>\n"
            "where options are:\n", prg_name);

    lo = long_options;

    largest = 0;
    for (; lo->name != NULL; lo++) {
        int len = strlen(lo->name) + 9;

        if (lo->has_arg == 1)
            len += sizeof("=ARG") - 1;
        else if (lo->has_arg == 2)
            len += sizeof("[=ARG]") - 1;

        if (largest < len)
            largest = len;
    }

    lo = long_options;
    help = help_texts;
    for (; lo->name != NULL; lo++, help++) {
        int len, i;

        fprintf(stderr, "\t-%c, --%s", lo->val, lo->name);
        len = strlen(lo->name) + 7;
        if (lo->has_arg == 1) {
            fputs("=ARG", stderr);
            len += sizeof("=ARG") - 1;
        } else if (lo->has_arg == 2) {
            fputs("[=ARG]", stderr);
            len += sizeof("[=ARG]") - 1;
        }

        for (i = len; i < largest; i++)
            fputc(' ', stderr);

        fputs("   ", stderr);
        fputs(*help, stderr);
        fputc('\n', stderr);
    }
    fputc('\n', stderr);
}

static void
print_array(const char * const *array)
{
    if (!array)
        return;

    for (; *array != NULL; array++)
        printf("\t\t%s\n", *array);
}

static void
print_parser(const char *path, const struct lms_parser_info *info)
{
    printf("parser: %s", path);

    if (!info) {
        fputs(" --- no information\n", stdout);
        return;
    }

    fputc('\n', stdout);
    printf("\tname.......: %s\n", info->name);
    printf("\tcategories.:\n");
    print_array(info->categories);
    printf("\tdescription: %s\n", info->description);
    printf("\tversion....: %s\n", info->version);
    printf("\tauthors....:\n");
    print_array(info->authors);
    printf("\turi........: %s\n", info->uri);
    fputc('\n', stdout);
}

static int
list_by_category(void *data, const char *path, const struct lms_parser_info *info)
{
    print_parser(path, info);
    return 1;
}

static int
list_parser(void *data, const char *path)
{
    struct lms_parser_info *info;

    info = lms_parser_info(path);
    print_parser(path, info);
    lms_parser_info_free(info);
    return 1;
}

static int
handle_options_independent(int argc, char **argv)
{
    int opt_index;

    opt_index = 0;
    while (1) {
        int c;

        c = getopt_long(argc, argv, short_options, long_options, &opt_index);
        if (c == -1)
            break;

        switch (c) {
        case '?':
            break;
        case 'h':
            show_help(argv[0]);
            return -1;
        case 'P':
            if (optarg) {
                printf("BEGIN: parsers of category: %s\n", optarg);
                lms_parsers_list_by_category(optarg, list_by_category, NULL);
                printf("END: parsers of category: %s\n", optarg);
            } else {
                puts("BEGIN: all parsers");
                lms_parsers_list(list_parser, NULL);
                puts("END: all parsers");
            }
            break;
        default:
            break;
        }
    }

    return 0;
}

static int
handle_options_setup(lms_t *lms, int argc, char **argv)
{
    int opt_index, parsers_added;

    optind = 0;
    opterr = 0;
    opt_index = 0;
    parsers_added = 0;
    while (1) {
        int c;

        c = getopt_long(argc, argv, short_options, long_options, &opt_index);
        if (c == -1)
            break;

        switch (c) {
        case 'p': {
            lms_plugin_t *p;
            if (optarg[0] == '.' || optarg[0] == '/')
                p = lms_parser_add(lms, optarg);
            else
                p = lms_parser_find_and_add(lms, optarg);
            if (!p)
                return -1;
            parsers_added = 1;
            break;
        }
        case 'c':
            if (lms_charset_add(lms, optarg) != 0)
                return -1;
            break;
        case 'i':
            lms_set_commit_interval(lms, atoi(optarg));
            break;
        case 't':
            lms_set_slave_timeout(lms, atoi(optarg));
            break;
        default:
            break;
        }
    }

    if (!parsers_added)
        fputs("WARNING: no parser added, --scan-path (-s) will not work "
              "as expected.\n", stderr);

    return 0;
}

static const char *
method_name(int method)
{
    switch (method) {
    case 1:
        return "mono";
    case 2:
        return "dual";
    default:
        return "unknown";
    }
}

static void
progress(lms_t *lms, const char *path, int path_len, lms_progress_status_t status, void *data)
{
    const char *s[] = {
        "UP_TO_DATE",
        "PROCESSED",
        "DELETED",
        "KILLED",
        "ERROR_PARSE",
        "ERROR_COMM",
    };
    const char *cstart = "", *cend = "", *name = data;

    if (color) {
        cend = "\033[0m";
        switch (status) {
        case LMS_PROGRESS_STATUS_UP_TO_DATE:
        case LMS_PROGRESS_STATUS_SKIPPED:
            break;
        case LMS_PROGRESS_STATUS_PROCESSED:
            cstart = "\033[32m";
            break;
        case LMS_PROGRESS_STATUS_DELETED:
            cstart = "\033[33m";
            break;
        case LMS_PROGRESS_STATUS_KILLED:
        case LMS_PROGRESS_STATUS_ERROR_PARSE:
        case LMS_PROGRESS_STATUS_ERROR_COMM:
            cstart = "\033[31m";
            break;
        }
    }

    printf("%s: %s => %d [%s%s%s]\n",
           name, path, status, cstart, s[status], cend);
}

static int
work(lms_t *lms, int method, int verbose, const char *path)
{
    int r;

    if (verbose) {
        lms_set_progress_callback(lms, progress, "CHECK", NULL);
        printf("CHECK at \"%s\" using %s method.\n", path, method_name(method));
    } else
        lms_set_progress_callback(lms, NULL, NULL, NULL);

    if (method == 1)
        r = lms_check_single_process(lms, path);
    else if (method == 2)
        r = lms_check(lms, path);
    else
        r = -1;

    if (r != 0) {
        if (verbose)
            printf("CHECK FAILED at \"%s\".\n", path);
        return r;
    }


    if (verbose) {
        lms_set_progress_callback(lms, progress, "PROGRESS", NULL);
        printf("PROCESS at \"%s\" using %s method.\n",
               path, method_name(method));
    } else
        lms_set_progress_callback(lms, NULL, NULL, NULL);

    if (method == 1)
        r = lms_process_single_process(lms, path);
    else if (method == 2)
        r = lms_process(lms, path);
    else
        r = -1;

    if (r != 0) {
        if (verbose)
            printf("PROCESS FAILED at \"%s\".\n", path);
        return r;
    }

    return 0;
}

static int
show(lms_t *lms, const char *orig_path)
{
    const char *sql = ("SELECT id, path, size FROM files WHERE path LIKE ? "
                       "AND dtime = 0");
    char buf[PATH_MAX + 2];
    char path[PATH_MAX];
    const char *db_path;
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int r, len;

    if (!realpath(orig_path, path)) {
        fprintf(stderr, "ERROR: could not realpath(\"%s\"): %s\n",
                orig_path, strerror(errno));
        return -1;
    }

    len = strlen(path);
    if (len + sizeof("/%") >= PATH_MAX) {
        fprintf(stderr, "ERROR: path is too long: \"%s\" + /%%\n", path);
        return -1;
    }

    db_path = lms_get_db_path(lms);
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not open DB \"%s\": %s\n",
                db_path, sqlite3_errmsg(db));
        r = -1;
        goto close_and_exit;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not prepare \"%s\": %s\n", sql,
                sqlite3_errmsg(db));
        r = -1;
        goto close_and_exit;
    }

    memcpy(buf, path, len);
    if (len > 0 && path[len - 1] != '/') {
        buf[len] = '/';
        len++;
    }
    buf[len] = '%';
    len++;
    buf[len] = '\0';

    if (sqlite3_bind_text(stmt, 1, buf, len, SQLITE_STATIC) != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not bind path query '%s'\n", buf);
        r = -1;
        goto close_and_exit;
    }

    printf("BEGIN: all files starting with directory '%s'\n", path);

    while ((r = sqlite3_step(stmt)) == SQLITE_ROW) {
        uint64_t id;
        const char *path;
        size_t size;

        id = sqlite3_column_int64(stmt, 0);
        path = sqlite3_column_blob(stmt, 1);
        size = sqlite3_column_int(stmt, 2);

        printf("%" PRIu64 " \"%s\" %zd\n", id, path, size);
    }
    printf("END: all files starting with directory '%s'\n", path);

    if (r == SQLITE_DONE)
        r = 0;
    else if (r == SQLITE_ERROR) {
        fprintf(stderr, "ERROR: could not step statement: %s\n",
                sqlite3_errmsg(db));
        r = -1;
    } else {
        fprintf(stderr, "ERROR: unknow step return value: %d\n", r);
        r = 0;
    }

    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
  close_and_exit:
    sqlite3_close(db);

    return r;
}

static int
handle_options_work(lms_t *lms, int argc, char **argv)
{
    int opt_index, method, verbose;

    verbose = 0;
    method = 2;
    optind = 0;
    opterr = 0;
    opt_index = 0;
    while (1) {
        int c;

        c = getopt_long(argc, argv, short_options, long_options, &opt_index);
        if (c == -1)
            break;

        switch (c) {
        case 'm':
            if (strcmp(optarg, "1") == 0 ||
                strcmp(optarg, "mono") == 0)
                method = 1;
            else if (strcmp(optarg, "2") == 0 ||
                     strcmp(optarg, "dual") == 0)
                method = 2;
            else
                fprintf(stderr,
                        "ERROR: invalid method=%s, should be 'mono' (1) or "
                        "'dual' (2). Default is dual.\n",
                        optarg);
        case 'v':
            if (optarg)
                verbose = !!atoi(optarg);
            else
                verbose = 1;
            break;
        case 's':
            if (work(lms, method, verbose, optarg) != 0)
                return -1;
            break;
        case 'S':
            show(lms, optarg);
            break;
        default:
            break;
        }
    }

    return 0;
}

int
main(int argc, char *argv[])
{
    int r;
    lms_t *lms;
    char *term;

    term = getenv("TERM");
    if (term &&
        (strncmp(term, "xterm", sizeof("xterm") - 1) == 0 ||
         strncmp(term, "linux", sizeof("linux") - 1) == 0))
        color = 1;

    r = handle_options_independent(argc, argv);
    if (r != 0)
        return r;

    if (optind >= argc) {
        fputs("ERROR: missing database path after options!\n", stderr);
        return -1;
    }

    lms = lms_new(argv[optind]);
    if (!lms) {
        fprintf(stderr, "ERROR: cannot open/create database %s\n",
                argv[optind]);
        return -1;
    }

    r = handle_options_setup(lms, argc, argv);
    if (r != 0)
        goto end;

    r = handle_options_work(lms, argc, argv);
    if (r != 0)
        goto end;

    fputs("success!\n", stderr);

  end:
    if (r)
        fputs("errors occurred, check out messages!\n", stderr);

    lms_free(lms);
    return r;
}
