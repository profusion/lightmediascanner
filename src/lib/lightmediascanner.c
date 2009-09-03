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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * @author Gustavo Sverzut Barbieri <gustavo.barbieri@openbossa.org>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "lightmediascanner.h"
#include "lightmediascanner_private.h"
#include "lightmediascanner_plugin.h"

#define DEFAULT_SLAVE_TIMEOUT 1000
#define DEFAULT_COMMIT_INTERVAL 100

static int
_parser_load(struct parser *p, const char *so_path)
{
    lms_plugin_t *(*plugin_open)(void);
    char *errmsg;

    memset(p, 0, sizeof(*p));

    p->dl_handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    errmsg = dlerror();
    if (errmsg) {
        fprintf(stderr, "ERROR: could not dlopen() %s\n", errmsg);
        return -1;
    }

    plugin_open = dlsym(p->dl_handle, "lms_plugin_open");
    errmsg = dlerror();
    if (errmsg) {
        fprintf(stderr, "ERROR: could not find plugin entry point %s\n",
                errmsg);
        return -2;
    }

    p->so_path = strdup(so_path);
    if (!p->so_path) {
        perror("strdup");
        return -3;
    }

    p->plugin = plugin_open();
    if (!p->plugin) {
        fprintf(stderr, "ERROR: plugin \"%s\" failed to init.\n", so_path);
        return -4;
    }

    return 0;
}

static int
_parser_unload(struct parser *p)
{
    int r;

    r = 0;
    if (p->plugin) {
        if (p->plugin->close(p->plugin) != 0) {
            fprintf(stderr, "ERROR: plugin \"%s\" failed to deinit.\n",
                    p->so_path);
            r -= 1;
        }
    }

    if (p->dl_handle) {
        char *errmsg;

        dlclose(p->dl_handle);
        errmsg = dlerror();
        if (errmsg) {
            fprintf(stderr, "ERROR: could not dlclose() plugin \"%s\": %s\n",
                    errmsg, p->so_path);
            r -= 1;
        }
    }

    if (p->so_path)
        free(p->so_path);

    return r;
}


/***********************************************************************
 * Public API.
 ***********************************************************************/
/**
 * Create new Light Media Scanner instance.
 *
 * @param db_path path to database file.
 * @return allocated data on success or NULL on failure.
 * @ingroup LMS_API
 */
lms_t *
lms_new(const char *db_path)
{
    lms_t *lms;

    lms = calloc(1, sizeof(lms_t));
    if (!lms) {
        perror("calloc");
        return NULL;
    }

    lms->cs_conv = lms_charset_conv_new();
    if (!lms->cs_conv) {
        free(lms);
        return NULL;
    }

    lms->commit_interval = DEFAULT_COMMIT_INTERVAL;
    lms->slave_timeout = DEFAULT_SLAVE_TIMEOUT;
    lms->db_path = strdup(db_path);
    if (!lms->db_path) {
        perror("strdup");
        lms_charset_conv_free(lms->cs_conv);
        free(lms);
        return NULL;
    }

    return lms;
}

/**
 * Free existing Light Media Scanner instance.
 *
 * @param lms previously allocated Light Media Scanner instance.
 *
 * @return On success 0 is returned.
 * @ingroup LMS_API
 */
int
lms_free(lms_t *lms)
{
    int i;

    if (!lms)
        return 0;

    if (lms->is_processing)
        return -1;

    if (lms->parsers) {
        for (i = 0; i < lms->n_parsers; i++)
            _parser_unload(lms->parsers + i);

        free(lms->parsers);
    }

    if (lms->progress.data && lms->progress.free_data)
        lms->progress.free_data(lms->progress.data);

    free(lms->db_path);
    lms_charset_conv_free(lms->cs_conv);
    free(lms);
    return 0;
}

/**
 * Set callback to be used to report progress (check and process).
 *
 * @param lms previously allocated Light Media Scanner instance.
 * @param cb function to call when files are processed or NULL to unset.
 * @param data data to give to cb when it's called, may be NULL.
 * @param free_data function to call to free @a data when lms is freed or
 *        new progress data is set.
 */
void
lms_set_progress_callback(lms_t *lms, lms_progress_callback_t cb, const void *data, lms_free_callback_t free_data)
{
    if (!lms) {
        if (data && free_data)
            free_data((void *)data);
        return;
    }

    if (lms->progress.data && lms->progress.free_data)
        lms->progress.free_data(lms->progress.data);

    lms->progress.cb = cb;
    lms->progress.data = (void *)data;
    lms->progress.free_data = free_data;
}

/**
 * Add parser plugin given it's shared object path.
 *
 * @param lms previously allocated Light Media Scanner instance.
 * @param so_path path to shared object (usable by dlopen(3)).
 *
 * @return On success the LMS handle to plugin is returned, NULL on error.
 * @ingroup LMS_API
 */
lms_plugin_t *
lms_parser_add(lms_t *lms, const char *so_path)
{
    struct parser *parser;

    if (!lms)
        return NULL;

    if (!so_path)
        return NULL;

    if (lms->is_processing) {
        fprintf(stderr, "ERROR: do not add parsers while it's processing.\n");
        return NULL;
    }

    lms->parsers = realloc(lms->parsers,
                           (lms->n_parsers + 1) * sizeof(struct parser));
    if (!lms->parsers) {
        perror("realloc");
        return NULL;
    }

    parser = lms->parsers + lms->n_parsers;
    if (_parser_load(parser, so_path) != 0) {
        _parser_unload(parser);
        return NULL;
    }

    lms->n_parsers++;
    return parser->plugin;
}

static int
lms_parser_find(char *buf, int buf_size, const char *name)
{
    int r;

    r = snprintf(buf, buf_size, "%s/%s.so", PLUGINSDIR, name);
    if (r >= buf_size)
        return 0;

    return 1;
}


/**
 * Add parser plugin given it's name.
 *
 * This will look at default plugin path by the file named @p name (plus
 * the required shared object extension).
 *
 * @param lms previously allocated Light Media Scanner instance.
 * @param name plugin name.
 *
 * @return On success the LMS handle to plugin is returned, NULL on error.
 * @ingroup LMS_API
 */
lms_plugin_t *
lms_parser_find_and_add(lms_t *lms, const char *name)
{
    char so_path[PATH_MAX];

    if (!lms)
        return NULL;
    if (!name)
        return NULL;

    if (!lms_parser_find(so_path, sizeof(so_path), name))
        return NULL;
    return lms_parser_add(lms, so_path);
}

int
lms_parser_del_int(lms_t *lms, int i)
{
    struct parser *parser;

    parser = lms->parsers + i;
    _parser_unload(parser);
    lms->n_parsers--;

    if (lms->n_parsers == 0) {
        free(lms->parsers);
        lms->parsers = NULL;
        return 0;
    } else {
        int dif;

        dif = lms->n_parsers - i;
        if (dif)
            lms->parsers = memmove(parser, parser + 1,
                                   dif * sizeof(struct parser));

        lms->parsers = realloc(lms->parsers,
                               lms->n_parsers * sizeof(struct parser));
        if (!lms->parsers) {
            lms->n_parsers = 0;
            return -1;
        }

        return 0;
    }
}

/**
 * Delete previously added parser, making it unavailable for future operations.
 *
 * @param lms previously allocated Light Media Scanner instance.
 *
 * @return On success 0 is returned.
 * @ingroup LMS_API
 */
int
lms_parser_del(lms_t *lms, lms_plugin_t *handle)
{
    int i;

    if (!lms)
        return -1;
    if (!handle)
        return -2;
    if (!lms->parsers)
        return -3;
    if (lms->is_processing) {
        fprintf(stderr, "ERROR: do not del parsers while it's processing.\n");
        return -4;
    }

    for (i = 0; i < lms->n_parsers; i++)
        if (lms->parsers[i].plugin == handle)
            return lms_parser_del_int(lms, i);

    return -3;
}

/**
 * Checks if Light Media Scanner is being used in a processing operation lile
 * lms_process() or lms_check().
 *
 * @param lms previously allocated Light Media Scanner instance.
 *
 * @return 1 if it is processing, 0 if it's not, -1 on error.
 * @ingroup LMS_API
 */
int
lms_is_processing(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_is_processing(NULL)\n");
        return -1;
    }

    return lms->is_processing;
}

/**
 * Get the database path given at creation time.
 *
 * @param lms previously allocated Light Media Scanner instance.
 *
 * @return path to database.
 * @ingroup LMS_API
 */
const char *
lms_get_db_path(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_get_db_path(NULL)\n");
        return NULL;
    }

    return lms->db_path;
}

/**
 * Get the maximum amount of milliseconds the slave can take to serve one file.
 *
 * If a slave takes more than this amount of milliseconds, it will be killed
 * and the scanner will continue with the next file.
 *
 * @param lms previously allocated Light Media Scanner instance.
 *
 * @return -1 on error or time in milliseconds otherwise.
 * @ingroup LMS_API
 */
int
lms_get_slave_timeout(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_get_slave_timeout(NULL)\n");
        return -1;
    }

    return lms->slave_timeout;
}

/**
 * Set the maximum amount of milliseconds the slave can take to serve one file.
 *
 * If a slave takes more than this amount of milliseconds, it will be killed
 * and the scanner will continue with the next file.
 *
 * @param lms previously allocated Light Media Scanner instance.
 * @param ms time in milliseconds.
 * @ingroup LMS_API
 */
void lms_set_slave_timeout(lms_t *lms, int ms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_set_slave_timeout(NULL, %d)\n", ms);
        return;
    }

    lms->slave_timeout = ms;
}

/**
 * Get the number of files served between database transactions.
 *
 * This is used as an optimization to database access: doing database commits
 * take some time and can slow things down too much, so you can choose to just
 * commit after some number of files are processed, this is the commit_interval.
 *
 * @param lms previously allocated Light Media Scanner instance.
 * @return (unsigned int)-1 on error, value otherwise.
 * @ingroup LMS_API
 */
unsigned int
lms_get_commit_interval(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_get_commit_interval(NULL)\n");
        return (unsigned int)-1;
    }

    return lms->commit_interval;
}

/**
 * Set the number of files served between database transactions.
 *
 * This is used as an optimization to database access: doing database commits
 * take some time and can slow things down too much, so you can choose to just
 * commit after @p transactions files are processed.
 *
 * @param lms previously allocated Light Media Scanner instance.
 * @param transactions number of files (transactions) to process between
 *        commits.
 * @ingroup LMS_API
 */
void
lms_set_commit_interval(lms_t *lms, unsigned int transactions)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_set_commit_interval(NULL, %u)\n",
                transactions);
        return;
    }

    lms->commit_interval = transactions;
}

/**
 * Register a new charset encoding to be used.
 *
 * All database text strings are in UTF-8, so one needs to register new
 * encodings in order to convert to it.
 *
 * @param lms previously allocated Light Media Scanner instance.
 * @param charset charset name as understood by iconv_open(3).
 *
 * @return On success 0 is returned.
 * @ingroup LMS_API
 */
int
lms_charset_add(lms_t *lms, const char *charset)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_charset_add(NULL)\n");
        return -1;
    }

    return lms_charset_conv_add(lms->cs_conv, charset);
}

/**
 * Forget about registered charset encoding.
 *
 * All database text strings are in UTF-8, so one needs to register new
 * encodings in order to convert to it.
 *
 * @param lms previously allocated Light Media Scanner instance.
 * @param charset charset name as understood by iconv_open(3).
 *
 * @return On success 0 is returned.
 * @ingroup LMS_API
 */
int
lms_charset_del(lms_t *lms, const char *charset)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_charset_del(NULL)\n");
        return -1;
    }

    return lms_charset_conv_del(lms->cs_conv, charset);
}

/**
 * List all known parsers on the system.
 *
 * No information is retrieved, you might like to call lms_parser_info()
 * on the callback path.
 *
 * @param cb function to call for each path found. If it returns 0,
 *        it stops iteraction.
 * @param data extra data to pass to @a cb on every call.
 */
void
lms_parsers_list(int (*cb)(void *data, const char *path), const void *data)
{
    void *datap = (void *)data;
    char path[PATH_MAX] = PLUGINSDIR;
    int base;
    DIR *d;
    struct dirent *de;

    if (!cb)
        return;

    base = sizeof(PLUGINSDIR) - 1;
    if (base + sizeof("/.so") >= PATH_MAX) {
        fprintf(stderr, "ERROR: path is too long '%s'\n", path);
        return;
    }

    d = opendir(path);
    if (!d) {
        fprintf(stderr, "ERROR: could not open directory %s: %s\n",
                path, strerror(errno));
        return;
    }

    path[base] = '/';
    base++;

    while ((de = readdir(d)) != NULL) {
        int len;

        if (de->d_name[0] == '.')
            continue;

        len = strlen(de->d_name);
        if (len < 3 || memcmp(de->d_name + len - 3, ".so", 3) != 0)
            continue;

        memcpy(path + base, de->d_name, len + 1); /* copy \0 */
        if (!cb(datap, path))
            break;
    }
    closedir(d);
}

struct lms_parsers_list_by_category_data {
    const char *category;
    int (*cb)(void *data, const char *path, const struct lms_parser_info *info);
    void *data;
};

static int
_lms_parsers_list_by_category(void *data, const char *path)
{
    struct lms_parsers_list_by_category_data *d = data;
    struct lms_parser_info *info;
    int r;

    info = lms_parser_info(path);
    if (!info)
        return 1;

    r = 1;
    if (info->categories) {
        const char * const *itr;
        for (itr = info->categories; *itr != NULL; itr++)
            if (strcmp(d->category, *itr) == 0) {
                r = d->cb(d->data, path, info);
                break;
            }
    }

    lms_parser_info_free(info);

    return r;
}

/**
 * List all known parsers of a given category.
 *
 * Since we need information to figure out parser category, these are
 * passed as argument to callback, but you should NOT modify or reference it
 * after callback function returns since it will be released after that.
 *
 * @param category which category to match.
 * @param cb function to call for each path found. If it returns 0,
 *        it stops iteraction.
 * @param data extra data to pass to @a cb on every call.
 */
void
lms_parsers_list_by_category(const char *category, int (*cb)(void *data, const char *path, const struct lms_parser_info *info), const void *data)
{
    struct lms_parsers_list_by_category_data d;

    if (!category || !cb)
        return;

    d.category = category;
    d.cb = cb;
    d.data = (void *)data;

    lms_parsers_list(_lms_parsers_list_by_category, &d);
}

static int
_lms_string_array_count(const char * const *array, int *size)
{
    int count, align_overflow;

    *size = 0;
    if (!array)
        return 0;

    count = 0;
    for (; *array != NULL; array++) {
        *size += sizeof(char *) + strlen(*array) + 1;
        count++;
    }
    if (count) {
        /* count NULL terminator */
        count++;
        *size += sizeof(char *);
    }

    align_overflow = *size % sizeof(char *);
    if (align_overflow)
        *size += sizeof(char *) - align_overflow;

    return count;
}

static void
_lms_string_array_copy(char **dst, const char * const *src, int count)
{
    char *d;

    d = (char *)(dst + count);

    for (; count > 1; count--, dst++, src++) {
        int len;

        len = strlen(*src) + 1;
        *dst = d;
        memcpy(*dst, *src, len);
        d += len;
    }

    *dst = NULL;
}

/**
 * Get parser information.
 *
 * Information can be used to let user choose parsers on Graphical User
 * Interfaces.
 *
 * @param so_path full path to module.
 * @see lms_parser_info_find()
 */
struct lms_parser_info *
lms_parser_info(const char *so_path)
{
    const struct lms_plugin_info *(*plugin_info)(void);
    const struct lms_plugin_info *pinfo;
    struct lms_parser_info *ret;
    const char *errmsg;
    void *dl_handle;
    int len, path_len, name_len, desc_len, ver_len, uri_len;
    int cats_count, cats_size, authors_count, authors_size;

    if (!so_path)
        return NULL;

    dl_handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    errmsg = dlerror();
    if (errmsg) {
        fprintf(stderr, "ERROR: could not dlopen() %s\n", errmsg);
        return NULL;
    }

    ret = NULL;
    plugin_info = dlsym(dl_handle, "lms_plugin_info");
    errmsg = dlerror();
    if (errmsg) {
        fprintf(stderr, "ERROR: could not find plugin info function %s\n",
                errmsg);
        goto close_and_exit;
    }

    if (!plugin_info) {
        fprintf(stderr, "ERROR: lms_plugin_info is NULL\n");
        goto close_and_exit;
    }

    pinfo = plugin_info();
    if (!pinfo) {
        fprintf(stderr, "ERROR: lms_plugin_info() returned NULL\n");
        goto close_and_exit;
    }

    path_len = strlen(so_path) + 1;
    name_len = pinfo->name ? strlen(pinfo->name) + 1 : 0;
    desc_len = pinfo->description ? strlen(pinfo->description) + 1 : 0;
    ver_len = pinfo->version ? strlen(pinfo->version) + 1 : 0;
    uri_len = pinfo->uri ? strlen(pinfo->uri) + 1 : 0;

    cats_count = _lms_string_array_count(pinfo->categories, &cats_size);
    authors_count = _lms_string_array_count(pinfo->authors, &authors_size);

    len = path_len + name_len + desc_len + ver_len + uri_len + cats_size +
        authors_size;
    ret = malloc(sizeof(*ret) + len);
    if (!ret) {
      fprintf(stderr, "ERROR: could not alloc %zd bytes: %s",
              sizeof(*ret) + len, strerror(errno));
      goto close_and_exit;
    }

    len = 0;

    if (cats_count) {
        ret->categories = (const char * const *)
            ((char *)ret + sizeof(*ret) + len);
        _lms_string_array_copy(
            (char **)ret->categories, pinfo->categories, cats_count);
        len += cats_size;
    } else
        ret->categories = NULL;

    if (authors_count) {
        ret->authors = (const char * const *)
            ((char *)ret + sizeof(*ret) + len);
        _lms_string_array_copy(
            (char **)ret->authors, pinfo->authors, authors_count);
        len += authors_size;
    } else
        ret->authors = NULL;

    ret->path = (char *)ret + sizeof(*ret) + len;
    memcpy((char *)ret->path, so_path, path_len);
    len += path_len;

    if (pinfo->name) {
        ret->name = (char *)ret + sizeof(*ret) + len;
        memcpy((char *)ret->name, pinfo->name, name_len);
        len += name_len;
    } else
        ret->name = NULL;

    if (pinfo->description) {
        ret->description = (char *)ret + sizeof(*ret) + len;
        memcpy((char *)ret->description, pinfo->description, desc_len);
        len += desc_len;
    } else
        ret->description = NULL;

    if (pinfo->version) {
        ret->version = (char *)ret + sizeof(*ret) + len;
        memcpy((char *)ret->version, pinfo->version, ver_len);
        len += ver_len;
    } else
        ret->version = NULL;

    if (pinfo->uri) {
        ret->uri = (char *)ret + sizeof(*ret) + len;
        memcpy((char *)ret->uri, pinfo->uri, uri_len);
        len += uri_len;
    } else
        ret->uri = NULL;

  close_and_exit:
    dlclose(dl_handle);
    return ret;
}

/**
 * Find parser by name and get its information.
 *
 * Information can be used to let user choose parsers on Graphical User
 * Interfaces.
 *
 * @param name name of .so to find the whole so_path and retrieve information.
 * @see lms_parser_info()
 */
struct lms_parser_info *
lms_parser_info_find(const char *name)
{
    char so_path[PATH_MAX];

    if (!name)
        return NULL;

    if (!lms_parser_find(so_path, sizeof(so_path), name))
        return NULL;

    return lms_parser_info(so_path);
}

/**
 * Free previously returned information.
 *
 * @note it is safe to call with NULL.
 */
void
lms_parser_info_free(struct lms_parser_info *info)
{
    free(info);
}
