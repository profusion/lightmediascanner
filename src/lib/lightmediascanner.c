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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    free(lms->db_path);
    lms_charset_conv_free(lms->cs_conv);
    free(lms);
    return 0;
}

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

lms_plugin_t *
lms_parser_find_and_add(lms_t *lms, const char *name)
{
    char so_path[PATH_MAX];

    if (!lms)
        return NULL;
    if (!name)
        return NULL;

    snprintf(so_path, sizeof(so_path), "%s/%s.so", PLUGINSDIR, name);
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

int
lms_is_processing(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_is_processing(NULL)\n");
        return -1;
    }

    return lms->is_processing;
}

const char *
lms_get_db_path(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_get_db_path(NULL)\n");
        return NULL;
    }

    return lms->db_path;
}

int
lms_get_slave_timeout(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_get_slave_timeout(NULL)\n");
        return -1;
    }

    return lms->slave_timeout;
}

void lms_set_slave_timeout(lms_t *lms, int ms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_set_slave_timeout(NULL, %d)\n", ms);
        return;
    }

    lms->slave_timeout = ms;
}

unsigned int
lms_get_commit_interval(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_get_commit_interval(NULL)\n");
        return (unsigned int)-1;
    }

    return lms->commit_interval;
}

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

int
lms_charset_add(lms_t *lms, const char *charset)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_charset_add(NULL)\n");
        return -1;
    }

    return lms_charset_conv_add(lms->cs_conv, charset);
}

int
lms_charset_del(lms_t *lms, const char *charset)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_charset_del(NULL)\n");
        return -1;
    }

    return lms_charset_conv_del(lms->cs_conv, charset);
}
