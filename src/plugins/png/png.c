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

/**
 * @brief
 *
 * Reads PNG images.
 *
 */

#include <lightmediascanner_plugin.h>
#include <lightmediascanner_utils.h>
#include <lightmediascanner_db.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static inline unsigned int
_chunk_to_uint(unsigned char *buf)
{
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static int
_png_data_get(int fd, struct lms_image_info *info)
{
    unsigned char buf[16], *p;
    const unsigned char sig[8] = {0x89, 0x50, 0x4e, 0x47, 0xd, 0xa, 0x1a, 0xa};
    const unsigned char ihdr[4] = {'I', 'H', 'D', 'R'};
    unsigned int length;

    if (read(fd, buf, sizeof(buf)) != sizeof(buf)) {
        perror("read");
        return -1;
    }

    if (memcmp(buf, sig, sizeof(sig)) != 0) {
        fprintf(stderr, "ERROR: invalid PNG signature.\n");
        return -2;
    }

    p = buf + sizeof(sig) + 4;
    if (memcmp(p, ihdr, sizeof(ihdr)) != 0) {
        fprintf(stderr, "ERROR: invalid first chunk: %4.4s.\n", p);
        return -3;
    }

    p = buf + sizeof(sig);
    length = _chunk_to_uint(p);
    if (length < 13) {
        fprintf(stderr, "ERROR: IHDR chunk size is too small: %d.\n", length);
        return -4;
    }

    if (read(fd, buf, 8) != 8) {
        perror("read");
        return -5;
    }
    info->width = _chunk_to_uint(buf);
    info->height = _chunk_to_uint(buf + 4);

    return 0;
}

static const char _name[] = "png";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".png")
};
static const char *_cats[] = {
    "multimedia",
    "picture",
    NULL
};
static const char *_authors[] = {
    "Gustavo Sverzut Barbieri",
    NULL
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_image_t *img_db;
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
    struct lms_image_info info = { };
    int fd, r;

    fd = open(finfo->path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (_png_data_get(fd, &info) != 0) {
        r = -2;
        goto done;
    }

    if (info.date == 0)
        info.date = finfo->mtime;

    if (!info.title.str) {
      long ext_idx;

      ext_idx = ((long)match) - 1;
      info.title.len = finfo->path_len - finfo->base - _exts[ext_idx].len;
      info.title.str = malloc((info.title.len + 1) * sizeof(char));
      memcpy(info.title.str, finfo->path + finfo->base, info.title.len);
      info.title.str[info.title.len] = '\0';
    }

    if (info.title.str)
      lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);
    if (info.artist.str)
      lms_charset_conv(ctxt->cs_conv, &info.artist.str, &info.artist.len);

    info.id = finfo->id;
    r = lms_db_image_add(plugin->img_db, &info);

  done:
    free(info.title.str);
    free(info.artist.str);

    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    close(fd);

    return r;
}

static int
_setup(struct plugin *plugin, struct lms_context *ctxt)
{
    plugin->img_db = lms_db_image_new(ctxt->db);
    if (!plugin->img_db)
        return -1;

    return 0;
}

static int
_start(struct plugin *plugin, struct lms_context *ctxt)
{
    return lms_db_image_start(plugin->img_db);
}

static int
_finish(struct plugin *plugin, struct lms_context *ctxt)
{
    if (plugin->img_db)
        return lms_db_image_free(plugin->img_db);

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

API const struct lms_plugin_info *
lms_plugin_info(void)
{
    static struct lms_plugin_info info = {
        _name,
        _cats,
        "PNG images",
        PACKAGE_VERSION,
        _authors,
        "http://lms.garage.maemo.org"
    };

    return &info;
}
