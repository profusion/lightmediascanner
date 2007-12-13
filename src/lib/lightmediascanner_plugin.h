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

/**
 * @defgroup LMS_Plugin Plugins-API
 *
 *
 * Plugins should implement the following call that provides required
 * callbacks (see lightmediascanner_plugin.h):
 *
 * @code
 *    struct lms_plugin *lms_plugin_open(void)
 * @endcode
 *
 * where:
 *
 * @code
 *    struct lms_plugin {
 *       const char *name;
 *       lms_plugin_match_fn_t match;
 *       lms_plugin_parse_fn_t parse;
 *       lms_plugin_close_fn_t close;
 *       lms_plugin_setup_fn_t setup;
 *       lms_plugin_start_fn_t start;
 *       lms_plugin_finish_fn_t finish;
 *    };
 * @endcode
 *
 * Users can add their own data to the end of this data
 * structure. Callbacks and their meanings are:
 *
 * @code
 *    void *match(lms_plugin_t *p,
 *                const char *path,
 *                int len,
 *                int base)
 * @endcode
 *
 *       Given the file 'path' of 'len' bytes, with base name starting at
 *       'base' bytes offset inside 'path', return a match. Non-NULL
 *       values means it matched, and this return will be given to
 *       parse() function so any match-time analysis can be reused.
 *       This function will be used in the slave process.
 *
 *
 * @code
 *    int parse(lms_plugin_t *p,
 *              struct lms_context *ctxt,
 *              const struct lms_file_info *finfo,
 *              void *match)
 * @endcode
 *
 *       Given the parsing context 'ctxt' (contains DB connection,
 *       charset conversion pointers and possible more), parse the file
 *       information 'finfo' using the previously matched data
 *       'match'. This should return 0 on success or other value for
 *       errors. This will be used in the slave process.
 *
 *
 * @code
 *    int close(lms_plugin_t *p)
 * @endcode
 *
 *       Closes the plugin returned by lms_plugin_open(), this will run
 *       on the master process.
 *
 *
 * @code
 *    int setup(lms_plugin_t *p, struct lms_context *ctxt)
 * @endcode
 *
 *       Prepare parser to be executed. This is the first phase of plugin
 *       initialization on the slave process, it should create database
 *       tables and like, after this function is called, no database
 *       schema changes are allowed!
 *
 *
 * @code
 *    int start(lms_plugin_t *p, struct lms_context *ctxt)
 * @endcode
 *
 *       This is the second phase of plugin initialization on the slave
 *       process.  At this point, all database tables should exist and
 *       database schema will not be changed anymore, so one can use this
 *       phase to compile SQL statements for future use.
 *
 *
 * @code
 *    int finish(lms_plugin_t *p, struct lms_context *ctxt)
 * @endcode
 *
 *       Finishes the plugin on slave process.
 *
 *
 * Although LMS doesn't place any restrictions on what plugins can do and
 * how they store information, it's good to have standard tables and easy
 * way to store data on them. For this task we provide
 * lightmediascanner_db.h with functions to add audios, images, videos,
 * playlists and possible more. Use should be pretty straightforward, see
 * existing plugins to see usage examples.
 *
 */

#ifndef _LIGHTMEDIASCANNER_PLUGIN_H_
#define _LIGHTMEDIASCANNER_PLUGIN_H_ 1

#include <lightmediascanner.h>
#include <lightmediascanner_charset_conv.h>
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
        size_t size; /**< file size in bytes */
    };

    struct lms_context {
        sqlite3 *db; /**< database instance */
        lms_charset_conv_t *cs_conv; /**< charset conversion tool */
    };

    typedef void *(*lms_plugin_match_fn_t)(lms_plugin_t *p, const char *path, int len, int base);
    typedef int (*lms_plugin_parse_fn_t)(lms_plugin_t *p, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match);
    typedef int (*lms_plugin_close_fn_t)(lms_plugin_t *p);
    typedef int (*lms_plugin_setup_fn_t)(lms_plugin_t *p, struct lms_context *ctxt);
    typedef int (*lms_plugin_start_fn_t)(lms_plugin_t *p, struct lms_context *ctxt);
    typedef int (*lms_plugin_finish_fn_t)(lms_plugin_t *p, struct lms_context *ctxt);

    struct lms_plugin {
        const char *name; /**< plugin name */
        lms_plugin_match_fn_t match; /**< check match */
        lms_plugin_parse_fn_t parse; /**< parse matched file */
        lms_plugin_close_fn_t close; /**< close plugin */
        lms_plugin_setup_fn_t setup; /**< setup (1st phase init) */
        lms_plugin_start_fn_t start; /**< start (2nd phase init) */
        lms_plugin_finish_fn_t finish; /**< finish plugin */
    };

#ifdef __cplusplus
}
#endif
#endif /* _LIGHTMEDIASCANNER_PLUGIN_H_ */
