/**
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * @author Gustavo Sverzut Barbieri <gustavo.barbieri@openbossa.org>
 */

#ifndef _LIGHTMEDIASCANNER_PRIVATE_H_
#define _LIGHTMEDIASCANNER_PRIVATE_H_ 1

#ifdef __GNUC__
# if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#  define GNUC_NON_NULL(...) __attribute__((nonnull(__VA_ARGS__)))
# else
#  define GNUC_NON_NULL(...)
# endif
#else
#  define GNUC_NON_NULL(...)
#endif

#include "lightmediascanner.h"
#include "lightmediascanner_charset_conv.h"
#include <sys/types.h>
#include <poll.h>
#include <limits.h>
#include <sqlite3.h>

#define PATH_SIZE PATH_MAX

struct fds {
    int r;
    int w;
};

/* info to be carried along lms_process() */
struct pinfo {
    struct fds master;
    struct fds slave;
    struct pollfd poll;
    lms_t *lms;
    pid_t child;
};

struct parser {
    lms_plugin_t *plugin;
    void *dl_handle;
    char *so_path;
};

struct lms {
    struct parser *parsers;
    int n_parsers;
    lms_charset_conv_t *cs_conv;
    char *db_path;
    int slave_timeout;
    unsigned int commit_interval;
    unsigned int is_processing:1;
};

struct db {
    sqlite3 *handle;
    sqlite3_stmt *transaction_begin;
    sqlite3_stmt *transaction_commit;
    sqlite3_stmt *transaction_rollback;
    sqlite3_stmt *get_file_info;
    sqlite3_stmt *insert_file_info;
    sqlite3_stmt *update_file_info;
    sqlite3_stmt *delete_file_info;
    sqlite3_stmt *clear_file_dtime;
};

int lms_parser_del_int(lms_t *lms, int i) GNUC_NON_NULL(1);


#endif /* _LIGHTMEDIASCANNER_PRIVATE_H_ */
