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
 * m3u playlist parser.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE 600
#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int
_m3u_get_n_entries(int fd, struct lms_playlist_info *info)
{
    char buf[1024];
    enum {
        IS_EMPTY,
        IS_ENTRY,
        IS_COMMENT
    } state;

    state = IS_EMPTY;
    do {
        ssize_t r;
        int i;

        r = read(fd, buf, sizeof(buf));
        if (r < 0) {
            perror("read");
            return -1;
        } else if (r == 0)
            goto done;

        for (i = 0; i < r; i++) {
            char c;

            c = buf[i];
            if (c == '\n') {
                if (state == IS_ENTRY)
                    info->n_entries++;
                state = IS_EMPTY;
            } else if (state == IS_EMPTY) {
                if (c == '#')
                    state = IS_COMMENT;
                else if (!isspace(c))
                    state = IS_ENTRY;
            }
        }
    } while (1);

  done:
    if (state == IS_ENTRY)
        info->n_entries++;
    return 0;
}

static const char _name[] = "m3u";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".m3u")
};
static const char *_cats[] = {
    "multimedia",
    "audio",
    "playlist",
    NULL
};
static const char *_authors[] = {
    "Gustavo Sverzut Barbieri",
    NULL
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_playlist_t *playlist_db;
};

static void *
_match(struct plugin *p, const char *path, int len, int base)
{
    long i;

    i = lms_which_extension(path, len, _exts, LMS_ARRAY_SIZE(_exts));
    if (i < 0)
      return NULL;
    else
      return (void*)(i + 1);
}

static int
_parse(struct plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    struct lms_playlist_info info = {0};
    int fd, r;
    long ext_idx;

    fd = open(finfo->path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (_m3u_get_n_entries(fd, &info) != 0)
        fprintf(stderr,
                "WARNING: could not get number of entries in playlist '%s'.\n",
                finfo->path);

    ext_idx = ((long)match) - 1;
    info.title.len = finfo->path_len - finfo->base - _exts[ext_idx].len;
    info.title.str = malloc((info.title.len + 1) * sizeof(char));
    memcpy(info.title.str, finfo->path + finfo->base, info.title.len);
    info.title.str[info.title.len] = '\0';
    lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);

    info.id = finfo->id;
    r = lms_db_playlist_add(plugin->playlist_db, &info);

    if (info.title.str)
        free(info.title.str);
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    close(fd);

    return r;
}

static int
_setup(struct plugin *plugin, struct lms_context *ctxt)
{
    plugin->playlist_db = lms_db_playlist_new(ctxt->db);
    if (!plugin->playlist_db)
        return -1;

    return 0;
}

static int
_start(struct plugin *plugin, struct lms_context *ctxt)
{
    return lms_db_playlist_start(plugin->playlist_db);
}

static int
_finish(struct plugin *plugin, struct lms_context *ctxt)
{
    if (plugin->playlist_db)
        return lms_db_playlist_free(plugin->playlist_db);

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

API struct lms_plugin_info *
lms_plugin_info(void)
{
    static struct lms_plugin_info info = {
        _name,
        _cats,
        "M3U playlists (aka WinAMP playlists)",
        PACKAGE_VERSION,
        _authors,
        "http://lms.garage.maemo.org"
    };

    return &info;
}
