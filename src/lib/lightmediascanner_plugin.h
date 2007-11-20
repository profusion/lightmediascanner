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

#ifndef _LIGHTMEDIASCANNER_PLUGIN_H_
#define _LIGHTMEDIASCANNER_PLUGIN_H_ 1

#ifdef API
#undef API
#endif

#ifdef __GNUC__
# if __GNUC__ >= 4
#  define API __attribute__ ((visibility("default")))
# else
#  define API
# endif
#else
#  define API
#endif

#include <lightmediascanner.h>
#include <sqlite3.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

    struct lms_file_info {
        const char *path; /**< file path */
        int path_len; /**< path length */
        int base; /**< index of basename inside path */
        int64_t id; /**< database id */
        time_t mtime; /**< in-disk modification time */
        time_t dtime; /**< deletion time */
    };

    typedef void *(*lms_plugin_match_fn_t)(lms_plugin_t *p, const char *path, int len, int base);
    typedef int (*lms_plugin_parse_fn_t)(lms_plugin_t *p, sqlite3 *db, const struct lms_file_info *finfo, void *match);
    typedef int (*lms_plugin_close_fn_t)(lms_plugin_t *p);
    typedef int (*lms_plugin_start_fn_t)(lms_plugin_t *p, sqlite3 *db);
    typedef int (*lms_plugin_finish_fn_t)(lms_plugin_t *p, sqlite3 *db);

    struct lms_plugin {
        const char *name;
        lms_plugin_match_fn_t match;
        lms_plugin_parse_fn_t parse;
        lms_plugin_close_fn_t close;
        lms_plugin_start_fn_t start;
        lms_plugin_finish_fn_t finish;
    };

#ifdef __cplusplus
}
#endif
#endif /* _LIGHTMEDIASCANNER_PLUGIN_H_ */
