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

#ifndef _LIGHTMEDIASCANNER_UTILS_H_
#define _LIGHTMEDIASCANNER_UTILS_H_ 1

#ifdef API
#undef API
#endif

#ifdef __GNUC__
# if __GNUC__ >= 4
#  define API __attribute__ ((visibility("default")))
# else
#  define API
# endif
# if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#  define GNUC_NON_NULL(...) __attribute__((nonnull(__VA_ARGS__)))
# else
#  define GNUC_NON_NULL(...)
# endif
#else
#  define API
#  define GNUC_NON_NULL(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif


    struct lms_string_size {
        char *str;
        unsigned int len;
    };

#define LMS_STATIC_STRING_SIZE(s)  {s, sizeof(s) - 1}
#define LMS_ARRAY_SIZE(a)  (sizeof(a) / sizeof(*a))


    API void lms_strstrip(char *str, unsigned int *p_len) GNUC_NON_NULL(1, 2);
    API void lms_strstrip_and_free(char **p_str, unsigned int *p_len) GNUC_NON_NULL(1, 2);
    API void lms_string_size_strip_and_free(struct lms_string_size *p) GNUC_NON_NULL(1);
    API int lms_which_extension(const char *name, unsigned int name_len, const struct lms_string_size *exts, unsigned int exts_len) GNUC_NON_NULL(1, 3);



#ifdef __cplusplus
}
#endif
#endif /* _LIGHTMEDIASCANNER_UTILS_H_ */
