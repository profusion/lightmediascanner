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

#include "lightmediascanner_charset_conv.h"
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

struct lms_charset_conv {
    iconv_t check;
    iconv_t fallback;
    unsigned int size;
    iconv_t *convs;
    char **names;
};

/**
 * Create a new charset conversion tool controlling its behavior.
 *
 * Conversion tool will try to convert provided strings to UTF-8, just need
 * to register known charsets with lms_charset_conv_add() and then call
 * lms_charset_conv().
 *
 * @return newly allocated conversion tool or NULL on error.
 */
lms_charset_conv_t *
lms_charset_conv_new_full(int use_check, int use_fallback)
{
    lms_charset_conv_t *lcc;

    lcc = malloc(sizeof(*lcc));
    if (!lcc) {
        perror("malloc");
        return NULL;
    }

    if (!use_check)
        lcc->check = (iconv_t)-1;
    else {
        lcc->check = iconv_open("UTF-8", "UTF-8");
        if (lcc->check == (iconv_t)-1) {
            perror("ERROR: could not create conversion checker");
            goto error_check;
        }
    }

    if (!use_fallback)
        lcc->fallback = (iconv_t)-1;
    else {
        lcc->fallback = iconv_open("UTF-8//IGNORE", "UTF-8");
        if (lcc->fallback == (iconv_t)-1) {
            perror("ERROR: could not create conversion fallback");
            goto error_fallback;
        }
    }

    lcc->size = 0;
    lcc->convs = NULL;
    lcc->names = NULL;
    return lcc;

  error_fallback:
    if (lcc->check != (iconv_t)-1)
        iconv_close(lcc->check);
  error_check:
    free(lcc);

    return NULL;
}

/**
 * Create a new charset conversion tool.
 *
 * Conversion tool will try to convert provided strings to UTF-8, just need
 * to register known charsets with lms_charset_conv_add() and then call
 * lms_charset_conv().
 *
 * @return newly allocated conversion tool or NULL on error.
 */
lms_charset_conv_t *
lms_charset_conv_new(void)
{
    return lms_charset_conv_new_full(1, 1);
}

/**
 * Free existing charset conversion tool.
 *
 * @param lcc existing Light Media Scanner charset conversion.
 */
void
lms_charset_conv_free(lms_charset_conv_t *lcc)
{
    unsigned int i;

    if (!lcc)
        return;

    if (lcc->check != (iconv_t)-1)
        iconv_close(lcc->check);
    if (lcc->fallback != (iconv_t)-1)
        iconv_close(lcc->fallback);

    for (i = 0; i < lcc->size; i++) {
        iconv_close(lcc->convs[i]);
        free(lcc->names[i]);
    }

    free(lcc->convs);
    free(lcc->names);
    free(lcc);
}

/**
 * Register new charset to conversion tool.
 *
 * @param lcc existing Light Media Scanner charset conversion.
 * @param charset charset name as understood by iconv_open(3).
 *
 * @return On success 0 is returned.
 */
int
lms_charset_conv_add(lms_charset_conv_t *lcc, const char *charset)
{
    iconv_t cd, *convs;
    char **names;
    int idx, ns;

    if (!lcc)
        return -1;

    if (!charset)
        return -2;

    cd = iconv_open("UTF-8", charset);
    if (cd == (iconv_t)-1) {
        fprintf(stderr, "ERROR: could not add conversion charset '%s': %s\n",
                charset, strerror(errno));
        return -3;
    }

    idx = lcc->size;
    ns = lcc->size + 1;

    convs = realloc(lcc->convs, ns * sizeof(*convs));
    if (!convs)
        goto realloc_error;
    lcc->convs = convs;
    lcc->convs[idx] = cd;

    names = realloc(lcc->names, ns * sizeof(*names));
    if (!names)
        goto realloc_error;
    lcc->names = names;
    lcc->names[idx] = strdup(charset);
    if (!lcc->names[idx])
        goto realloc_error;

    lcc->size = ns;
    return 0;

  realloc_error:
    perror("realloc");
    iconv_close(cd);
    return -4;
}

static int
_find(const lms_charset_conv_t *lcc, const char *charset)
{
    unsigned int i;

    for (i = 0; i < lcc->size; i++)
        if (strcmp(lcc->names[i], charset) == 0)
            return i;

    return -1;
}

/**
 * Forget about previously registered charset in conversion tool.
 *
 * @param lcc existing Light Media Scanner charset conversion.
 * @param charset charset name.
 *
 * @return On success 0 is returned.
 */
int
lms_charset_conv_del(lms_charset_conv_t *lcc, const char *charset)
{
    iconv_t *convs;
    char **names;
    int idx;

    if (!lcc)
        return -1;

    if (!charset)
        return -2;

    idx = _find(lcc, charset);
    if (idx < 0) {
        fprintf(stderr, "ERROR: could not find charset '%s'\n", charset);
        return -3;
    }

    iconv_close(lcc->convs[idx]);
    free(lcc->names[idx]);

    lcc->size--;
    for (; (unsigned)idx < lcc->size; idx++) {
        lcc->convs[idx] = lcc->convs[idx + 1];
        lcc->names[idx] = lcc->names[idx + 1];
    }

    convs = realloc(lcc->convs, lcc->size * sizeof(*convs));
    if (convs)
        lcc->convs = convs;
    else
        perror("could not realloc 'convs'");

    names = realloc(lcc->names, lcc->size * sizeof(*names));
    if (names)
        lcc->names = names;
    else
        perror("could not realloc 'names'");

    return 0;
}

static int
_check(lms_charset_conv_t *lcc, const char *istr, unsigned int ilen, char *ostr, unsigned int olen)
{
    char *inbuf, *outbuf;
    size_t r, inlen, outlen;

    if (lcc->check == (iconv_t)-1)
        return -1;

    inbuf = (char *)istr;
    inlen = ilen;
    outbuf = ostr;
    outlen = olen;

    iconv(lcc->check, NULL, NULL, NULL, NULL);
    r = iconv(lcc->check, &inbuf, &inlen, &outbuf, &outlen);
    if (r == (size_t)-1)
        return -1;
    else
        return 0;
}

static int
_conv(iconv_t cd, char **p_str, unsigned int *p_len, char *ostr, unsigned int olen)
{
    char *inbuf, *outbuf;
    size_t r, inlen, outlen;

    inbuf = *p_str;
    inlen = *p_len;
    outbuf = ostr;
    outlen = olen;

    iconv(cd, NULL, NULL, NULL, NULL);
    r = iconv(cd, &inbuf, &inlen, &outbuf, &outlen);
    if (r == (size_t)-1)
        return -1;

    *p_len = olen - outlen;
    free(*p_str);
    *p_str = ostr;

    outbuf = realloc(*p_str, *p_len + 1);
    if (outbuf)
        *p_str = outbuf;
    else {
        perror("realloc");
        if (*p_len > 0)
            (*p_len)--;
        else {
            free(*p_str);
            *p_str = NULL;
            return 0;
        }
    }

    (*p_str)[*p_len] = '\0';
    return 0;
}

static void
_fix_non_ascii(char *s, int len)
{
    for (; len > 0; len--, s++)
        if (!isprint(*s))
            *s = '?';
}

/**
 * If required, do charset conversion to UTF-8.
 *
 * @param lcc existing Light Media Scanner charset conversion.
 * @param p_str string to be converted.
 * @param p_len string size.
 *
 * @note the check for string being already UTF-8 is not reliable,
 *       some cases might show false positives (UTF-16 is considered UTF-8).
 * @see lms_charset_conv_check()
 *
 * @return On success 0 is returned.
 */
int
lms_charset_conv(lms_charset_conv_t *lcc, char **p_str, unsigned int *p_len)
{
    char *outstr;
    int i, outlen;

    if (!lcc)
        return -1;
    if (!p_str)
        return -2;
    if (!p_len)
        return -3;
    if (!*p_str || !*p_len)
        return 0;

    outlen = 2 * *p_len;
    outstr = malloc(outlen + 1);
    if (!outstr) {
        perror("malloc");
        return -4;
    }

    if (_check(lcc, *p_str, *p_len, outstr, outlen) == 0) {
        free(outstr);
        return 0;
    }

    for (i = 0; (unsigned) i < lcc->size; i++)
        if (_conv(lcc->convs[i], p_str, p_len, outstr, outlen) == 0)
            return 0;

    if (lcc->fallback == (iconv_t)-1) {
        free(outstr);
        return -5;
    }

    fprintf(stderr,
            "WARNING: could not convert '%*s' to any charset, use fallback\n",
            *p_len, *p_str);
    i = _conv(lcc->fallback, p_str, p_len, outstr, outlen);
    if (i < 0) {
        _fix_non_ascii(*p_str, *p_len);
        free(outstr);
    }
    return i;
}

/**
 * Forcefully do charset conversion to UTF-8.
 *
 * @param lcc existing Light Media Scanner charset conversion.
 * @param p_str string to be converted.
 * @param p_len string size.
 *
 * @note This function does not check for the string being in UTF-8 before
 *       doing the conversion, use it if you are sure about the charset.
 *       In this case you'll usually have just one charset added.
 *
 * @return On success 0 is returned.
 */
int
lms_charset_conv_force(lms_charset_conv_t *lcc, char **p_str, unsigned int *p_len)
{
    char *outstr;
    int i, outlen;

    if (!lcc)
        return -1;
    if (!p_str)
        return -2;
    if (!p_len)
        return -3;
    if (!*p_str || !*p_len)
        return 0;

    outlen = 2 * *p_len;
    outstr = malloc(outlen + 1);
    if (!outstr) {
        perror("malloc");
        return -4;
    }

    for (i = 0; (unsigned)i < lcc->size; i++)
        if (_conv(lcc->convs[i], p_str, p_len, outstr, outlen) == 0)
            return 0;

    if (lcc->fallback == (iconv_t)-1) {
        free(outstr);
        return -5;
    }

    fprintf(stderr,
            "WARNING: could not convert '%*s' to any charset, use fallback\n",
            *p_len, *p_str);
    i = _conv(lcc->fallback, p_str, p_len, outstr, outlen);
    if (i < 0) {
        _fix_non_ascii(*p_str, *p_len);
        free(outstr);
    }
    return i;
}

/**
 * Check if strings is not UTF-8 and conversion is required.
 *
 * @param lcc existing Light Media Scanner charset conversion.
 * @param str string to be analysed.
 * @param len string size.
 *
 * @note current implementation is not reliable, it tries to convert from
 *       UTF-8 to UTF-8. Some cases, like ISO-8859-1 will work, but some like
 *       UTF-16 to UTF-8 will say it's already in the correct charset,
 *       even if it's not.
 *
 * @return 0 if string is already UTF-8.
 */
int
lms_charset_conv_check(lms_charset_conv_t *lcc, const char *str, unsigned int len)
{
    char *outstr;
    int r, outlen;

    if (!lcc)
        return -1;
    if (!str || !len)
        return 0;

    outlen = 2 * len;
    outstr = malloc(outlen);
    if (!outstr) {
        perror("malloc");
        return -2;
    }

    r = _check(lcc, str, len, outstr, outlen);
    free(outstr);
    return r;
}
