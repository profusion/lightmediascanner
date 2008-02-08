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

#ifndef _LIGHTMEDIASCANNER_CHARSET_CONV_H_
#define _LIGHTMEDIASCANNER_CHARSET_CONV_H_ 1

#ifdef GNUC_MALLOC
#undef GNUC_MALLOC
#endif
#ifdef GNUC_WARN_UNUSED_RESULT
#undef GNUC_WARN_UNUSED_RESULT
#endif
#ifdef GNUC_NON_NULL
#undef GNUC_NON_NULL
#endif
#ifdef API
#undef API
#endif

#ifdef __GNUC__
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
#  define GNUC_MALLOC __attribute__((__malloc__))
# else
#  define GNUC_MALLOC
# endif
# if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#  define GNUC_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#  define GNUC_NON_NULL(...) __attribute__((nonnull(__VA_ARGS__)))
# else
#  define GNUC_WARN_UNUSED_RESULT
#  define GNUC_NON_NULL(...)
# endif
# if __GNUC__ >= 4
#  define API __attribute__ ((visibility("default")))
# else
#  define API
# endif
#else
#  define GNUC_MALLOC
#  define GNUC_WARN_UNUSED_RESULT
#  define GNUC_NON_NULL(...)
#  define API
#endif

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @defgroup LMS_CHARSET Charset Conversion
 *
 * Utilities to convert strings to UTF-8, the charset used in database.
 * @{
 */

    typedef struct lms_charset_conv lms_charset_conv_t;

    API lms_charset_conv_t *lms_charset_conv_new_full(int use_check, int use_fallback) GNUC_MALLOC GNUC_WARN_UNUSED_RESULT;
    API lms_charset_conv_t *lms_charset_conv_new(void) GNUC_MALLOC GNUC_WARN_UNUSED_RESULT;
    API void lms_charset_conv_free(lms_charset_conv_t *lcc) GNUC_NON_NULL(1);
    API int lms_charset_conv_add(lms_charset_conv_t *lcc, const char *charset) GNUC_NON_NULL(1, 2);
    API int lms_charset_conv_del(lms_charset_conv_t *lcc, const char *charset) GNUC_NON_NULL(1, 2);

    API int lms_charset_conv(lms_charset_conv_t *lcc, char **p_str, unsigned int *p_len) GNUC_NON_NULL(1, 2, 3);
    API int lms_charset_conv_force(lms_charset_conv_t *lcc, char **p_str, unsigned int *p_len) GNUC_NON_NULL(1, 2, 3);
    API int lms_charset_conv_check(lms_charset_conv_t *lcc, const char *str, unsigned int len) GNUC_NON_NULL(1, 2);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif
#endif /* _LIGHTMEDIASCANNER_CHARSET_CONV_H_ */
