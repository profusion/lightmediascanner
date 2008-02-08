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

#include <lightmediascanner_utils.h>
#include <ctype.h>
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
    int i;
    unsigned int *exts_pos;
    const char *s;

    exts_pos = alloca(exts_len * sizeof(*exts_pos));
    for (i = 0; i < exts_len; i++)
        exts_pos[i] = exts[i].len;

    for (s = name + name_len - 1; s >= name; s--) {
        int i, match;
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
