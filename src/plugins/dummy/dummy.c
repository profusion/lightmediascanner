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
 * @brief
 *
 * Dummy plugin demonstrating the basic lightmediascanner_plugin API,
 * it just write paths to /tmp/dummy.log.
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

struct plugin {
    struct lms_plugin plugin;
    int fd;
};

static void *
_match(struct plugin *p, const char *path, int len, int base)
{
    return (void*)1;
}

static int
_parse(struct plugin *plugin, const struct lms_file_info *finfo, void *match)
{
    write(plugin->fd, finfo->path, finfo->path_len);
    write(plugin->fd, "\n", 1);
    return 0;
}

static int
_close(struct plugin *plugin)
{
    if (close(plugin->fd) != 0)
        perror("close");

    free(plugin);
    return 0;
}

API struct lms_plugin *
lms_plugin_open(void)
{
    struct plugin *plugin;
    char logfile[PATH_MAX];

    snprintf(logfile, sizeof(logfile), "/tmp/dummy-%d.log", getuid());

    plugin = malloc(sizeof(*plugin));
    plugin->plugin.name = _name;
    plugin->plugin.match = (lms_plugin_match_fn_t)_match;
    plugin->plugin.parse = (lms_plugin_parse_fn_t)_parse;
    plugin->plugin.close = (lms_plugin_close_fn_t)_close;
    plugin->fd = open(logfile, O_WRONLY | O_CREAT, 0600);
    if (plugin->fd < 0) {
        perror("open");
        free(plugin);
        return NULL;
    }

    return (struct lms_plugin *)plugin;
}
