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
 * Video Dummy plugin, just register matched extensions in videos DB.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE 600
#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char _name[] = "video-dummy";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".avi"),
    LMS_STATIC_STRING_SIZE(".mpg"),
    LMS_STATIC_STRING_SIZE(".mpeg"),
    LMS_STATIC_STRING_SIZE(".rm"),
    LMS_STATIC_STRING_SIZE(".3gp"),
    LMS_STATIC_STRING_SIZE(".rm"),
    LMS_STATIC_STRING_SIZE(".ram"),
    LMS_STATIC_STRING_SIZE(".mp4"),
    LMS_STATIC_STRING_SIZE(".ogm")
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_video_t *video_db;
};

static void *
_match(struct plugin *p, const char *path, int len, int base)
{
    int i;

    i = lms_which_extension(path, len, _exts, LMS_ARRAY_SIZE(_exts));
    if (i < 0)
        return NULL;
    else
        return (void*)(i + 1);
}

static int
_parse(struct plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    struct lms_video_info info = {0};
    int r, ext_idx;

    ext_idx = ((int)match) - 1;
    info.title.len = finfo->path_len - finfo->base - _exts[ext_idx].len;
    info.title.str = malloc((info.title.len + 1) * sizeof(char));
    memcpy(info.title.str, finfo->path + finfo->base, info.title.len);
    info.title.str[info.title.len] = '\0';

    info.id = finfo->id;
    r = lms_db_video_add(plugin->video_db, &info);

    free(info.title.str);

    return r;
}

static int
_setup(struct plugin *plugin, struct lms_context *ctxt)
{
    plugin->video_db = lms_db_video_new(ctxt->db);
    if (!plugin->video_db)
        return -1;

    return 0;
}

static int
_start(struct plugin *plugin, struct lms_context *ctxt)
{
    return lms_db_video_start(plugin->video_db);
}

static int
_finish(struct plugin *plugin, struct lms_context *ctxt)
{
    if (plugin->video_db)
        return lms_db_video_free(plugin->video_db);

    return 0;
}


static int
_close(struct plugin *plugin)
{
    free(plugin);
    return 0;
}

API struct lms_plugin *
lms_plugin_open(void)
{
    struct plugin *plugin;

    plugin = malloc(sizeof(*plugin));
    plugin->plugin.name = _name;
    plugin->plugin.match = (lms_plugin_match_fn_t)_match;
    plugin->plugin.parse = (lms_plugin_parse_fn_t)_parse;
    plugin->plugin.close = (lms_plugin_close_fn_t)_close;
    plugin->plugin.setup = (lms_plugin_setup_fn_t)_setup;
    plugin->plugin.start = (lms_plugin_start_fn_t)_start;
    plugin->plugin.finish = (lms_plugin_finish_fn_t)_finish;

    return (struct lms_plugin *)plugin;
}
