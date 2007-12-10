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

#ifndef _LIGHTMEDIASCANNER_H_
#define _LIGHTMEDIASCANNER_H_ 1

#ifdef API
#undef API
#endif

#ifdef __GNUC__
# if __GNUC__ >= 4
#  define API __attribute__ ((visibility("default")))
#  define GNUC_NULL_TERMINATED __attribute__((__sentinel__))
# else
#  define API
#  define GNUC_NULL_TERMINATED
# endif
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
#  define GNUC_PURE __attribute__((__pure__))
#  define GNUC_MALLOC __attribute__((__malloc__))
#  define GNUC_CONST __attribute__((__const__))
#  define GNUC_UNUSED __attribute__((__unused__))
# else
#  define GNUC_PURE
#  define GNUC_MALLOC
#  define GNUC_NORETURN
#  define GNUC_CONST
#  define GNUC_UNUSED
# endif
# if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#  define GNUC_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#  define GNUC_NON_NULL(...) __attribute__((nonnull(__VA_ARGS__)))
# else
#  define GNUC_WARN_UNUSED_RESULT
#  define GNUC_NON_NULL(...)
# endif
#else
#  define API
#  define GNUC_NULL_TERMINATED
#  define GNUC_PURE
#  define GNUC_MALLOC
#  define GNUC_CONST
#  define GNUC_UNUSED
#  define GNUC_WARN_UNUSED_RESULT
#  define GNUC_NON_NULL(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct lms lms_t;
    typedef struct lms_plugin lms_plugin_t;

    API lms_t *lms_new(const char *db_path) GNUC_MALLOC GNUC_WARN_UNUSED_RESULT;
    API int lms_free(lms_t *lms) GNUC_NON_NULL(1);
    API int lms_process(lms_t *lms, const char *top_path) GNUC_NON_NULL(1, 2);
    API int lms_check(lms_t *lms, const char *top_path) GNUC_NON_NULL(1, 2);
    API const char *lms_get_db_path(const lms_t *lms) GNUC_NON_NULL(1);
    API int lms_is_processing(const lms_t *lms) GNUC_PURE GNUC_NON_NULL(1);
    API int lms_get_slave_timeout(const lms_t *lms) GNUC_NON_NULL(1);
    API void lms_set_slave_timeout(lms_t *lms, int ms) GNUC_NON_NULL(1);
    API unsigned int lms_get_commit_interval(const lms_t *lms) GNUC_NON_NULL(1);
    API void lms_set_commit_interval(lms_t *lms, unsigned int transactions) GNUC_NON_NULL(1);

    API lms_plugin_t *lms_parser_add(lms_t *lms, const char *so_path) GNUC_NON_NULL(1, 2);
    API lms_plugin_t *lms_parser_find_and_add(lms_t *lms, const char *name) GNUC_NON_NULL(1, 2);
    API int lms_parser_del(lms_t *lms, lms_plugin_t *handle) GNUC_NON_NULL(1, 2);

    API int lms_charset_add(lms_t *lms, const char *charset) GNUC_NON_NULL(1, 2);
    API int lms_charset_del(lms_t *lms, const char *charset) GNUC_NON_NULL(1, 2);

#ifdef __cplusplus
}
#endif
#endif /* _LIGHTMEDIASCANNER_H_ */
