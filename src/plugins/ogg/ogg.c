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
 *
 * Reference:
 *   http://xiph.org/ogg/doc/libogg/decoding.html
 *   http://xiph.org/vorbis/doc/libvorbis/overview.html
 */

#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>
#include <lightmediascanner_utils.h>
#include <shared/util.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <theora/theoradec.h>

#include "lms_ogg_private.h"

#ifndef CHUNKSIZE
int CHUNKSIZE = 4096;
#endif

#define MAX_CHUNKS_PER_PAGE 10

struct stream {
    struct lms_stream base;
    int serial;
    int remain_headers;
    ogg_stream_state *os;
    union {
        struct {
            vorbis_comment vc;
            vorbis_info vi;
        } audio;
        struct {
            th_comment tc;
            th_info ti;
            th_setup_info *tsi;
        } video;
    };
};

struct ogg_info {
    struct lms_string_size title;
    struct lms_string_size artist;
    struct lms_string_size album;
    struct lms_string_size genre;
    enum lms_stream_type type;
    unsigned char trackno;
    unsigned int channels;
    unsigned int sampling_rate;
    unsigned int bitrate;

    struct stream *streams;
};

static const struct lms_string_size _container = LMS_STATIC_STRING_SIZE("ogg");
static const struct lms_string_size _audio_codec =
    LMS_STATIC_STRING_SIZE("vorbis");
static const struct lms_string_size _video_codec =
    LMS_STATIC_STRING_SIZE("theora");

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

static void
_set_lms_info(struct lms_string_size *info, const char *tag)
{
    int size;

    if (!info || !tag)
        return;

    size = strlen(tag);

    if (!size)
        return;

    info->len = size;
    info->str = malloc(size * sizeof(char));
    memcpy(info->str, tag, size);
    lms_string_size_strip_and_free(info);
}

static bool _ogg_read_page(FILE *fp, ogg_sync_state *osync, ogg_page *page)
{
    int i;

    for (i = 0; i < MAX_CHUNKS_PER_PAGE && ogg_sync_pageout(osync, page) != 1;
         i++) {
        lms_ogg_buffer_t buffer = lms_get_ogg_sync_buffer(osync, CHUNKSIZE);
        int bytes = fread(buffer, 1, CHUNKSIZE, fp);

        /* EOF */
        if (bytes == 0)
            return false;

        ogg_sync_wrote(osync, bytes);
    }

    if (i > MAX_CHUNKS_PER_PAGE)
        return false;

    return true;
}

static struct stream *_stream_new(int serial, int id)
{
    struct stream *s;

    s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    s->serial = serial;
    s->os = lms_create_ogg_stream(serial);

    s->base.type = LMS_STREAM_TYPE_UNKNOWN;
    s->base.stream_id = id;

    return s;
}

static void _stream_free(struct stream *s)
{
    switch (s->base.type) {
    case LMS_STREAM_TYPE_UNKNOWN:
    case LMS_STREAM_TYPE_SUBTITLE:
        break;
    case LMS_STREAM_TYPE_AUDIO:
        vorbis_comment_clear(&s->audio.vc);
        vorbis_info_clear(&s->audio.vi);
        break;
    case LMS_STREAM_TYPE_VIDEO:
        th_comment_clear(&s->video.tc);
        th_info_clear(&s->video.ti);
        th_setup_free(s->video.tsi);
        free(s->base.video.aspect_ratio.str);
        break;
    }

    lms_destroy_ogg_stream(s->os);
    free(s);
}

static struct stream *_info_find_stream(struct ogg_info *info, int serial)
{
    struct stream *s;

    for (s = info->streams; s; s = (struct stream *) s->base.next) {
        if (s->serial == serial)
            return s;
    }

    return NULL;
}

static struct stream *_info_prepend_stream(struct ogg_info *info, int serial,
                                           int id)
{
    struct stream *s = _stream_new(serial, id);
    if (!s)
        return NULL;
    s->base.next = (struct lms_stream *) info->streams;
    info->streams = s;
    return s;
}

static int _stream_handle_page(struct stream *s, ogg_page *page)
{
    ogg_packet packet;
    int r;

    if (!s->os)
        s->os = lms_create_ogg_stream(s->serial);

    if (ogg_stream_pagein(s->os, page) < 0)
        return -1;

    do {
        r = ogg_stream_packetout(s->os, &packet);
        if (r == 0)
            return 1;
        if (r == -1)
            return -1;

        switch (s->base.type) {
        case LMS_STREAM_TYPE_UNKNOWN:
            th_info_init(&s->video.ti);
            th_comment_init(&s->video.tc);
            s->video.tsi = NULL;
            if (th_decode_headerin(&s->video.ti, &s->video.tc, &s->video.tsi,
                                   &packet) != TH_ENOTFORMAT) {
                s->base.type = LMS_STREAM_TYPE_VIDEO;
                s->remain_headers = 2;
            } else {
                th_info_clear(&s->video.ti);
                th_comment_clear(&s->video.tc);
                if (s->video.tsi)
                    th_setup_free(s->video.tsi);
                vorbis_info_init(&s->audio.vi);
                vorbis_comment_init(&s->audio.vc);
                if (vorbis_synthesis_headerin(&s->audio.vi, &s->audio.vc,
                                              &packet) != 0) {
                    vorbis_info_clear(&s->audio.vi);
                    vorbis_comment_clear(&s->audio.vc);
                    s->remain_headers = -1;
                    return 1;
                }

                s->base.type = LMS_STREAM_TYPE_AUDIO;
                s->remain_headers = 2;
            }
            break;
        case LMS_STREAM_TYPE_VIDEO:
            assert(s->remain_headers > 0);
            r = th_decode_headerin(&s->video.ti, &s->video.tc, &s->video.tsi,
                                   &packet);
            if (r < 0) {
                s->remain_headers = -1;
                return 1;
            }

            s->remain_headers--;
            if (!s->remain_headers)
                return 0;
            break;
        case LMS_STREAM_TYPE_AUDIO:
            assert(s->remain_headers > 0);
            r = vorbis_synthesis_headerin(&s->audio.vi, &s->audio.vc, &packet);
            if (r != 0) {
                s->remain_headers = -1;
                return 1;
            }

            s->remain_headers--;
            if (!s->remain_headers)
                return 0;
            break;
        default:
            s->remain_headers = -1;
            return 1;
        }
    } while (1);

    return 1;
}

static void _parse_theora_and_vorbis_streams(struct ogg_info *info,
                                             struct stream *video_stream)
{
    struct stream *s, *prev, *next;
    const char *tag;

    /* filter unknown or incomplete streams */

    for (s = info->streams, next = NULL; s; s = next) {
        next = (struct stream *) s->base.next;
        if (s->base.type != LMS_STREAM_TYPE_UNKNOWN && s->remain_headers == 0)
            break;
        _stream_free(s);
    }

    info->streams = s;
    if (!s)
        return;

    for (prev = s, s = next; s; s = next) {
        next = (struct stream *) s->base.next;
        if (s->base.type != LMS_STREAM_TYPE_UNKNOWN && s->remain_headers == 0) {
            prev = s;
        } else {
            prev->base.next = (struct lms_stream *) next;
            _stream_free(s);
        }
    }

    /* add information to each stream */
    for (s = info->streams; s; s = (struct stream *) s->base.next) {
        if (s->base.type == LMS_STREAM_TYPE_AUDIO) {
            s->base.codec = _audio_codec;
            s->base.audio.channels = s->audio.vi.channels;
            s->base.audio.bitrate = s->audio.vi.bitrate_nominal;
        } else if (s->base.type == LMS_STREAM_TYPE_VIDEO) {
            unsigned int num, den;

            s->base.codec = _video_codec;
            s->base.video.bitrate = s->video.ti.target_bitrate;
            s->base.video.width = s->video.ti.frame_width;
            s->base.video.height = s->video.ti.frame_height;
            num = s->video.ti.fps_numerator;
            den = s->video.ti.fps_denominator;
            if (num && den)
                s->base.video.framerate = (double)(num) / den;

            num = s->video.ti.aspect_numerator;
            den = s->video.ti.aspect_denominator;
            if (num && den) {
                reduce_gcd(num, den, &num, &den);
                asprintf(&s->base.video.aspect_ratio.str, "%u:%u", num, den);
                s->base.video.aspect_ratio.len =
                    s->base.video.aspect_ratio.str ?
                    strlen(s->base.video.aspect_ratio.str) : 0;
            }
        }
    }

    /* query the first video stream about relevant metdata */
    tag = th_comment_query(&video_stream->video.tc, (char *) "TITLE", 0);
    _set_lms_info(&info->title, tag);

    tag = th_comment_query(&video_stream->video.tc, (char *) "ARTIST", 0);
    _set_lms_info(&info->artist, tag);
}

static void _parse_vorbis_stream(struct ogg_info *info, struct stream *s)
{
    const char *tag;

    info->channels = s->audio.vi.channels;
    info->sampling_rate = s->audio.vi.rate;
    info->bitrate = s->audio.vi.bitrate_nominal;

    tag = vorbis_comment_query(&s->audio.vc, "TITLE", 0);
    _set_lms_info(&info->title, tag);

    tag = vorbis_comment_query(&s->audio.vc, "ARTIST", 0);
    _set_lms_info(&info->artist, tag);

    tag = vorbis_comment_query(&s->audio.vc, "ALBUM", 0);
    _set_lms_info(&info->album, tag);

    tag = vorbis_comment_query(&s->audio.vc, "GENRE", 0);
    _set_lms_info(&info->genre, tag);

    tag = vorbis_comment_query(&s->audio.vc, "TRACKNUMBER", 0);
    if (tag)
        info->trackno = atoi(tag);
}

static int _parse_ogg(const char *filename, struct ogg_info *info)
{
    FILE *fp;
    ogg_page page;
    ogg_sync_state *osync;
    int r = 0;
    /* no numeration in the protocol, start arbitrarily from 1 */
    int id = 0;
    /* the 1st audio stream, the one used if audio */
    struct stream *s, *audio_stream = NULL, *video_stream = NULL;

    if (!filename)
        return -1;

    fp = fopen(filename, "rb");
    if (fp == NULL)
        return -1;

    /* Skip ID3 on the beginning */
    fseek(fp, _id3_tag_size(fp), SEEK_SET);

    osync = lms_create_ogg_sync();
    while (_ogg_read_page(fp, osync, &page)) {
        int serial = ogg_page_serialno(&page);

        s = _info_find_stream(info, serial);

        /* A new page for a stream that has all the headers already */
        if (s) {
            if (s->remain_headers == 0)
                break;
            else if (s->remain_headers < 0)
                continue;
        } else {
            /* We didn't find the stream, but we are not at its start page
             * neither: it's an unknown stream, go to the next one */
            if (!ogg_page_bos(&page))
                continue;

            s = _info_prepend_stream(info, serial, ++id);
            if (!s) {
                r = -ENOMEM;
                goto done;
            }
        }

        r = _stream_handle_page(s, &page);
        if (r < 0)
            goto done;
        if (r > 0)
            continue;

        if (s->remain_headers == 0) {
            if (s->base.type == LMS_STREAM_TYPE_AUDIO && !audio_stream)
                audio_stream = s;
            else if (s->base.type == LMS_STREAM_TYPE_VIDEO && !video_stream)
                video_stream = s;
        }
    }

    if (video_stream) {
        _parse_theora_and_vorbis_streams(info, video_stream);
        info->type = LMS_STREAM_TYPE_VIDEO;
    } else if (audio_stream) {
        _parse_vorbis_stream(info, audio_stream);
        info->type = LMS_STREAM_TYPE_AUDIO;
    }

done:
    lms_destroy_ogg_sync(osync);
    fclose(fp);

    return r;
}


static const char _name[] = "ogg";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".ogg"),
    LMS_STATIC_STRING_SIZE(".oga"),
    LMS_STATIC_STRING_SIZE(".ogv")
};
static const char *_cats[] = {
    "multimedia",
    "audio",
    NULL
};
static const char *_authors[] = {
    "Renato Chencarek",
    "Eduardo Lima (Etrunko)",
    NULL
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
    lms_db_video_t *video_db;
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
_parse(struct plugin *plugin, struct lms_context *ctxt,
       const struct lms_file_info *finfo, void *match)
{
    struct ogg_info info = { .type = LMS_STREAM_TYPE_UNKNOWN };
    int r;

    r = _parse_ogg(finfo->path, &info);
    if (r != 0)
      goto done;

    if (!info.title.str)
        info.title = str_extract_name_from_path(finfo->path, finfo->path_len,
                                                finfo->base,
                                                &_exts[((long) match) - 1],
                                                NULL);
    if (info.title.str)
        lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);
    if (info.artist.str)
        lms_charset_conv(ctxt->cs_conv, &info.artist.str, &info.artist.len);

    if (info.type == LMS_STREAM_TYPE_AUDIO) {
        struct lms_audio_info audio_info = { };

        if (info.album.str)
            lms_charset_conv(ctxt->cs_conv, &info.album.str, &info.album.len);
        if (info.genre.str)
            lms_charset_conv(ctxt->cs_conv, &info.genre.str, &info.genre.len);

        audio_info.id = finfo->id;
        audio_info.title = info.title;
        audio_info.artist = info.artist;
        audio_info.album = info.album;
        audio_info.genre = info.genre;

        audio_info.trackno = info.trackno;
        audio_info.container = _container;
        audio_info.codec = _audio_codec;
        audio_info.channels = info.channels;
        audio_info.sampling_rate = info.sampling_rate;
        audio_info.bitrate = info.bitrate;

        r = lms_db_audio_add(plugin->audio_db, &audio_info);
    } else if (info.type == LMS_STREAM_TYPE_VIDEO) {
        struct lms_video_info video_info = { };

        video_info.id = finfo->id;
        video_info.title = info.title;
        video_info.artist = info.artist;
        video_info.streams = (struct lms_stream *) info.streams;
        r = lms_db_video_add(plugin->video_db, &video_info);
    }

done:
    while (info.streams) {
        struct stream *s = info.streams;
        info.streams = (struct stream *) s->base.next;
        _stream_free(s);
    }

    free(info.title.str);
    free(info.artist.str);
    free(info.album.str);
    free(info.genre.str);

    return r;
}

static int
_setup(struct plugin *plugin, struct lms_context *ctxt)
{
    plugin->audio_db = lms_db_audio_new(ctxt->db);
    if (!plugin->audio_db)
        return -1;
    plugin->video_db = lms_db_video_new(ctxt->db);
    if (!plugin->video_db)
        return -1;

    return 0;
}

static int
_start(struct plugin *plugin, struct lms_context *ctxt)
{
    int r;
    r = lms_db_audio_start(plugin->audio_db);
    r |= lms_db_video_start(plugin->video_db);
    return r;
}

static int
_finish(struct plugin *plugin, struct lms_context *ctxt)
{
    if (plugin->audio_db)
        lms_db_audio_free(plugin->audio_db);
    if (plugin->video_db)
        lms_db_video_free(plugin->video_db);
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

API const struct lms_plugin_info *
lms_plugin_info(void)
{
    static struct lms_plugin_info info = {
        _name,
        _cats,
        "OGG files",
        PACKAGE_VERSION,
        _authors,
        "http://lms.garage.maemo.org"
    };

    return &info;
}
