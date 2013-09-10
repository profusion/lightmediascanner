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

#include <lightmediascanner_utils.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>

/**
 * Strips string, in place.
 *
 * @param str string to be stripped.
 * @param p_len string length to analyse, also the place where the final size
 *        is stored.
 */
void
lms_strstrip(char *str, unsigned int *p_len)
{
    int i, len;
    char *p;

    len = *p_len;

    if (len == 0)
        return;

    if (*str == '\0') {
        *p_len = 0;
        return;
    }

    p = str + len - 1;
    for (i = len - 1; i >= 0; i--) {
        if (isspace(*p) || *p == '\0') {
            *p = '\0';
            len--;
            p--;
        } else
            break;
    }
    if (len == 0) {
        *p_len = 0;
        return;
    }

    p = str;
    for (i = 0; i < len; i++) {
        if (isspace(*p))
            p++;
        else
            break;
    }
    len -= i;
    if (len == 0) {
        *str = '\0';
        *p_len = 0;
        return;
    }

    *p_len = len;

    if (str < p)
        for (; len >= 0; len--, str++, p++)
            *str = *p;
}

/**
 * If string exists, strips it, in place, free if *p_len = 0
 *
 * @param p_str pointer to string to be stripped.
 * @param p_len string length to analyse, also the place where the final size
 *        is stored.
 *
 * @note this will call free() on *p_str if it becomes empty.
 */
void
lms_strstrip_and_free(char **p_str, unsigned int *p_len)
{
    if (!*p_str)
        return;

    lms_strstrip(*p_str, p_len);
    if (*p_len == 0) {
        free(*p_str);
        *p_str = NULL;
    }
}

/**
 * lms_string_size version of lms_strstrip_and_free().
 *
 * @param *p pointer to lms_string_size to be stripped.
 *
 * @note this will call free() on lms_string_size->str if it becomes empty.
 */
void
lms_string_size_strip_and_free(struct lms_string_size *p)
{
    if (!p->str)
        return;

    lms_strstrip(p->str, &p->len);
    if (p->len == 0) {
        free(p->str);
        p->str = NULL;
    }
}

/**
 * lms_string_size version of strdup().
 *
 * @param dst where to return the duplicated value.
 * @param src pointer to lms_string_size to be duplicated.
 *
 * @return 1 on success, 0 on failure.
 */
int
lms_string_size_dup(struct lms_string_size *dst, const struct lms_string_size *src)
{
    if (src->len == 0) {
        dst->str = NULL;
        dst->len = 0;
        return 1;
    }

    dst->str = malloc(src->len + 1);
    if (!dst->str) {
        dst->len = 0;
        return 0;
    }

    dst->len = src->len;
    memcpy(dst->str, src->str, dst->len);
    dst->str[dst->len] = '\0';
    return 1;
}

/**
 * Similar to lms_string_size_dup(), but from a simple string.
 *
 * @param dst where to return the duplicated value.
 * @param src pointer to string to be duplicated.
 * @param size size to copy or -1 to auto-calculate.
 *
 * @return 1 on success, 0 on failure.
 */
int
lms_string_size_strndup(struct lms_string_size *dst, const char *src, int size)
{
    if (size < 0) {
        if (!src)
            size = 0;
        else
            size = strlen(src);
    }

    if (size == 0) {
        dst->str = NULL;
        dst->len = 0;
        return 1;
    }

    dst->str = malloc(size + 1);
    if (!dst->str) {
        dst->len = 0;
        return 0;
    }

    dst->len = size;
    memcpy(dst->str, src, dst->len);
    dst->str[dst->len] = '\0';
    return 1;
}

/**
 * Find out which of the given extensions matches the given name.
 *
 * @param name string to analyse.
 * @param name_len string length.
 * @param exts array of extensions to be checked.
 * @param exts_len number of items in array @p exts
 *
 * @return index in @p exts or -1 if it doesn't match none.
 */
int
lms_which_extension(const char *name, unsigned int name_len, const struct lms_string_size *exts, unsigned int exts_len) {
    unsigned int i;
    unsigned int *exts_pos;
    const char *s;

    exts_pos = alloca(exts_len * sizeof(*exts_pos));
    for (i = 0; i < exts_len; i++)
        exts_pos[i] = exts[i].len;

    for (s = name + name_len - 1; s >= name; s--) {
        int match;
        char c1, c2;

        c1 = *s;
        if (c1 >= 'a')
            c2 = c1;
        else
            c2 = 'a' + c1 - 'A';

        match = 0;
        for (i = 0; i < exts_len; i++) {
            if (exts_pos[i] > 0) {
                char ce;

                ce = exts[i].str[exts_pos[i] - 1];
                if (ce == c1 || ce == c2) {
                    if (exts_pos[i] == 1)
                        return i;
                    exts_pos[i]--;
                    match = 1;
                } else
                    exts_pos[i] = 0;
            }
        }
        if (!match)
            return -1;
    }

    return -1;
}
