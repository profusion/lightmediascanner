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

/**
 * @brief
 *
 * Dummy plugin that stores all files in the database.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE 600
#include <lightmediascanner_plugin.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

static const char _name[] = "dummy";
static const char *_cats[] = {
    "all",
    "audio",
    "video",
    "picture",
    NULL
};
static const char *_authors[] = {
    "Gustavo Sverzut Barbieri",
    NULL
};

static void *
_match(struct lms_plugin *p, const char *path, int len, int base)
{
    return (void*)1;
}

static int
_parse(struct lms_plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    return 0;
}

static int
_close(struct lms_plugin *plugin)
{
    return 0;
}

static int
_setup(struct lms_plugin *plugin,  struct lms_context *ctxt)
{
    return 0;
}

static int
_start(struct lms_plugin *plugin, struct lms_context *ctxt)
{
    return 0;
}

static int
_finish(struct lms_plugin *plugin, struct lms_context *ctxt)
{
    return 0;
}

API struct lms_plugin *
lms_plugin_open(void)
{
    struct lms_plugin *plugin;

    plugin = malloc(sizeof(struct lms_plugin));
    plugin->name = _name;
    plugin->match = _match;
    plugin->parse = _parse;
    plugin->close = _close;
    plugin->setup = _setup;
    plugin->start = _start;
    plugin->finish = _finish;

    return plugin;
}

API struct lms_plugin_info *
lms_plugin_info(void)
{
    static struct lms_plugin_info info = {
        _name,
        _cats,
        "Stores all files in the database with no processing",
        PACKAGE_VERSION,
        _authors,
        "http://lms.garage.maemo.org"
    };

    return &info;
}
