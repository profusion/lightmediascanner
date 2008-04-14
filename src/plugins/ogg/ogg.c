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
 * @author Renato Chencarek <renato.chencarek@openbossa.org>
 * @author Eduardo Lima (Etrunko) <eduardo.lima@indt.org.br>
 *
 */

/**
 * @brief
 *
 * ogg file parser.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>
#include <stdlib.h>
#include <string.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

static long int
_id3_tag_size(FILE *file)
{
    unsigned char tmp[4];
    long int size;

    if (fread(tmp, 1, 4, file) == 4) {
        if (tmp[0] == 'I' && tmp[1] == 'D' &&
            tmp[2] == '3' && tmp[3] < 0xFF) {
            fseek(file, 2, SEEK_CUR);
            if (fread(tmp, 1, 4, file) == 4) {
                size = 10 +   ( (long)(tmp[3])
                              | ((long)(tmp[2]) << 7)
                              | ((long)(tmp[1]) << 14)
                              | ((long)(tmp[0]) << 21) );

                return size;
            }
        }
    }
    return 0L;
}


static int
_get_vorbis_comment(FILE *file, vorbis_comment *vc)
{
    char *buffer;
    int bytes, i, chunks = 0;
    ogg_packet header;

    ogg_page og;
    ogg_sync_state osync;
    ogg_stream_state os;
    vorbis_info vi;

    int serial;
    int CHUNKSIZE = 4096;
    int nheaders = 0;

    ogg_sync_init(&osync);

    while (1) {
        buffer = ogg_sync_buffer(&osync, CHUNKSIZE);
        bytes = fread(buffer, 1, CHUNKSIZE, file);

        ogg_sync_wrote(&osync, bytes);

        if (ogg_sync_pageout(&osync, &og) == 1)
            break;

        if (chunks++ >= 10)
            return -1;
    }

    serial = ogg_page_serialno(&og);

    ogg_stream_init(&os, serial);
    vorbis_info_init(&vi);
    vorbis_comment_init(vc);

    if (ogg_stream_pagein(&os, &og) < 0)
        return -1;
    if (ogg_stream_packetout(&os, &header) != 1)
        return -1;
    if (vorbis_synthesis_headerin(&vi, vc, &header) != 0)
        return -1;

    i = 1;
    nheaders = 3;
    while (i < nheaders) {
        while (i < nheaders) {
            int result = ogg_sync_pageout(&osync, &og);
            if (result == 0)
                break;
            else if (result == 1) {
                ogg_stream_pagein(&os, &og);
                while (i < nheaders) {
                    result = ogg_stream_packetout(&os, &header);
                    if (result == 0)
                        break;
                    if (result == -1)
                        return -1;

                    vorbis_synthesis_headerin(&vi, vc, &header);
                    i++;
                }
            }
        }

        buffer = ogg_sync_buffer(&osync, CHUNKSIZE);
        bytes = fread(buffer, 1, CHUNKSIZE, file);

        if (bytes == 0 && i < 2)
            return -1;

        ogg_sync_wrote(&osync, bytes);
    }

    ogg_stream_clear(&os);
    ogg_sync_clear(&osync);
    vorbis_info_clear(&vi);

    return 0;
}

static int
_parse_ogg(const char *filename, struct lms_audio_info *info)
{
    vorbis_comment vc;
    FILE *file;
    const char *tag;
    int size;

    if (!filename)
        return -1;

    file = fopen(filename, "rb");
    if (file == NULL)
        return -1;

    fseek(file, _id3_tag_size(file), SEEK_SET);

    if (_get_vorbis_comment(file, &vc) != 0)
        return -1;

    tag = vorbis_comment_query(&vc, "TITLE", 0);
    size = strlen(tag);
    if (tag && size > 0) {
        info->title.len = size;
        info->title.str = malloc(size * sizeof(char));
        memcpy(info->title.str, tag, size);
        lms_string_size_strip_and_free(&info->title);
    }

    tag = vorbis_comment_query(&vc, "ARTIST", 0);
    size = strlen(tag);
    if (tag && size > 0) {
        info->artist.len = size;
        info->artist.str = malloc(size * sizeof(char));
        memcpy(info->artist.str, tag, size);
        lms_string_size_strip_and_free(&info->artist);
    }

    tag = vorbis_comment_query(&vc, "ALBUM", 0);
    size = strlen(tag);
    if (tag && size > 0) {
        info->album.len = size;
        info->album.str = malloc(size * sizeof(char));
        memcpy(info->album.str, tag, size);
        lms_string_size_strip_and_free(&info->album);
    }

    tag = vorbis_comment_query(&vc, "TRACKNUMBER", 0);
    if (tag)
        info->trackno = atoi(tag);

    tag = vorbis_comment_query(&vc, "GENRE", 0);
    size = strlen(tag);
    if (tag && size > 0) {
        info->genre.len = size;
        info->genre.str = malloc(size * sizeof(char));
        memcpy(info->genre.str, tag, size);
        lms_string_size_strip_and_free(&info->genre);
    }

    fclose(file);
    vorbis_comment_clear(&vc);

    return 0;
}


static const char _name[] = "ogg";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".ogg")
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
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
    struct lms_audio_info info = {0};
    int r;

    r = _parse_ogg(finfo->path, &info);
    if (r != 0)
      goto done;

    if (!info.title.str) {
      int ext_idx;

      ext_idx = ((int)match) - 1;
      info.title.len = finfo->path_len - finfo->base - _exts[ext_idx].len;
      info.title.str = (char *)malloc((info.title.len + 1) * sizeof(char));
      memcpy(info.title.str, finfo->path + finfo->base, info.title.len);
      info.title.str[info.title.len] = '\0';
    }

    if (info.title.str)
      lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);
    if (info.artist.str)
      lms_charset_conv(ctxt->cs_conv, &info.artist.str, &info.artist.len);
    if (info.album.str)
      lms_charset_conv(ctxt->cs_conv, &info.album.str, &info.album.len);
    if (info.genre.str)
      lms_charset_conv(ctxt->cs_conv, &info.genre.str, &info.genre.len);

    info.id = finfo->id;
    r = lms_db_audio_add(plugin->audio_db, &info);

 done:
    if (info.title.str)
        free(info.title.str);
    if (info.artist.str)
        free(info.artist.str);
    if (info.album.str)
        free(info.album.str);
    if (info.genre.str)
        free(info.genre.str);

    return r;
}

static int
_setup(struct plugin *plugin, struct lms_context *ctxt)
{
    plugin->audio_db = lms_db_audio_new(ctxt->db);
    if (!plugin->audio_db)
        return -1;

    return 0;
}

static int
_start(struct plugin *plugin, struct lms_context *ctxt)
{
    return lms_db_audio_start(plugin->audio_db);
}

static int
_finish(struct plugin *plugin, struct lms_context *ctxt)
{
    if (plugin->audio_db)
        return lms_db_audio_free(plugin->audio_db);

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

    plugin = (struct plugin *)malloc(sizeof(*plugin));
    plugin->plugin.name = _name;
    plugin->plugin.match = (lms_plugin_match_fn_t)_match;
    plugin->plugin.parse = (lms_plugin_parse_fn_t)_parse;
    plugin->plugin.close = (lms_plugin_close_fn_t)_close;
    plugin->plugin.setup = (lms_plugin_setup_fn_t)_setup;
    plugin->plugin.start = (lms_plugin_start_fn_t)_start;
    plugin->plugin.finish = (lms_plugin_finish_fn_t)_finish;

    return (struct lms_plugin *)plugin;
}
