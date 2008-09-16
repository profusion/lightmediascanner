/**
 * Copyright (C) 2008 by ProFUSION embedded systems
 * Copyright (C) 2007 by INdT
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * @author Gustavo Sverzut Barbieri <barbieri@profusion.mobi>
 * @author Gustavo Sverzut Barbieri <gustavo.barbieri@openbossa.org>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <lightmediascanner.h>
#include <stdio.h>
#include <stdlib.h>

void
usage(const char *prgname)
{
    /*
     * EXAMPLE: rm -f test.db && ./test 1000 1000 test.db id3 UTF-8 ~/music
     */
    fprintf(stderr,
            "Usage:\n"
            "\t%s <api-select> <commit-interval> <slave-timeout> <db-path>"
            " <parser> <charset> <scan-path>,\n\n"
            "where api-select may be:\n"
            "\t0,  for dual process lms (main API)\n"
            "\t1,  for single process lms (secondary API)\n",
            prgname);
}

void
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
    printf("%s => %d [%s]\n", path, status, s[status]);
}

int
main(int argc, char *argv[])
{
    char *db_path, *parser_name, *charset, *scan_path;
    int (*process)(lms_t *lms, const char *top_path);
    int commit_interval, slave_timeout;
    lms_plugin_t *parser;
    lms_t *lms;
    int type;

    if (argc < 7) {
        usage(argv[0]);
        return 1;
    }

    type = atoi(argv[1]);
    commit_interval = atoi(argv[2]);
    slave_timeout = atoi(argv[3]);
    db_path = argv[4];
    parser_name = argv[5];
    charset = argv[6];
    scan_path = argv[7];

    if (type == 0)
        process = lms_process;
    else
        process = lms_process_single_process;

    lms = lms_new(db_path);
    if (!lms) {
        fprintf(stderr,
                "ERROR: could not create light media scanner for DB \"%s\".\n",
                db_path);
        return -1;
    }

    lms_set_commit_interval(lms, commit_interval);
    lms_set_slave_timeout(lms, slave_timeout);
    lms_set_progress_callback(lms, progress, NULL, NULL);

    parser = lms_parser_find_and_add(lms, parser_name);
    if (!parser) {
        fprintf(stderr, "ERROR: could not create parser \"%s\".\n",
                parser_name);
        lms_free(lms);
        return -2;
    }

    if (lms_charset_add(lms, charset) != 0) {
        fprintf(stderr, "ERROR: could not add charset '%s'\n", charset);
        lms_free(lms);
        return -3;
    }

    printf("checking: %s\n", scan_path);
    if (lms_check(lms, scan_path) != 0) {
        fprintf(stderr, "ERROR: checking \"%s\".\n", scan_path);
        lms_free(lms);
        return -4;
    }

    printf("process: %s\n", scan_path);
    if (process(lms, scan_path) != 0) {
        fprintf(stderr, "ERROR: processing \"%s\".\n", scan_path);
        lms_free(lms);
        return -5;
    }

    if (lms_free(lms) != 0) {
        fprintf(stderr, "ERROR: could not close light media scanner.\n");
        return -6;
    }

    return 0;
}
