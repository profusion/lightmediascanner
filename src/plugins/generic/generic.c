/**
 * Copyright (C) 2013 by Intel Corporation
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
 * @author Leandro Dorileo <leandro.maciel.dorileo@intel.com>
 */

#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include "libavutil/opt.h"

#include <lightmediascanner_db.h>
#include <lightmediascanner_dlna.h>
#include <lightmediascanner_plugin.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#define DECL_STR(cname, str)                                            \
    static const struct lms_string_size cname = LMS_STATIC_STRING_SIZE(str)

static const char _name[] = "generic";

static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".mpeg"),
    LMS_STATIC_STRING_SIZE(".mpg"),
    LMS_STATIC_STRING_SIZE(".mpeg2"),
    LMS_STATIC_STRING_SIZE(".mp2"),
    LMS_STATIC_STRING_SIZE(".mp3"),
    LMS_STATIC_STRING_SIZE(".adts"),
    LMS_STATIC_STRING_SIZE(".m3u"),
    LMS_STATIC_STRING_SIZE(".mp4"),
    LMS_STATIC_STRING_SIZE(".wma"),
    LMS_STATIC_STRING_SIZE(".ogg"),
};

DECL_STR(_codec_mpeg1layer3, "mpeg1layer3");
DECL_STR(_container_3gp, "3gp");
DECL_STR(_container_mp4, "mp4");
DECL_STR(_container_ogg, "ogg");

DECL_STR(_container_audio_wmav1, "wmav1");
DECL_STR(_container_audio_wmav2, "wmav2");
DECL_STR(_container_audio_wmavpro, "wmavpro");

DECL_STR(_codec_video_theora, "theora");
DECL_STR(_codec_audio_vorbis, "vorbis");
DECL_STR(_codec_audio_asf, "asf");
DECL_STR(_codec_audio_mpeg4aac_main, "mpeg4aac-main");
DECL_STR(_codec_audio_mpeg4aac_lc, "mpeg4aac-lc");
DECL_STR(_codec_audio_mpeg4aac_ssr, "mpeg4aac-ssr");
DECL_STR(_codec_audio_mpeg4aac_ltp, "mpeg4aac-ltp");
DECL_STR(_codec_audio_mpeg4aac_he, "mpeg4aac-he");
DECL_STR(_codec_audio_mpeg4aac_scalable, "mpeg4aac-scalable");

typedef void (* generic_codec_container_cb)(AVStream *stream, struct lms_string_size *value);

struct codec_container {
    unsigned int id;
    generic_codec_container_cb get_codec;
    generic_codec_container_cb get_container;
};

struct codec_container_descriptor {
    unsigned int id;
    const struct lms_string_size *desc;
};

static const struct codec_container_descriptor _codec_list[] = {
    {AV_CODEC_ID_MP3, &_codec_mpeg1layer3},
    {AV_CODEC_ID_WMAV1, &_codec_audio_asf},
    {AV_CODEC_ID_WMAV2, &_codec_audio_asf},
    {AV_CODEC_ID_WMAPRO, &_codec_audio_asf},
    {AV_CODEC_ID_VORBIS, &_codec_audio_vorbis},
    {AV_CODEC_ID_THEORA, &_codec_video_theora},
};

static const struct codec_container_descriptor _container_list[] = {
    {AV_CODEC_ID_MPEG2VIDEO, &_container_mp4},
    {AV_CODEC_ID_AAC, &_container_3gp},
    {AV_CODEC_ID_WMAV1, &_container_audio_wmav1},
    {AV_CODEC_ID_WMAV2, &_container_audio_wmav2},
    {AV_CODEC_ID_WMAPRO, &_container_audio_wmavpro},
    {AV_CODEC_ID_H264, &_container_mp4},
    {AV_CODEC_ID_VORBIS, &_container_ogg},
    {AV_CODEC_ID_THEORA, &_container_ogg},
};

static void
_mp4_get_audio_codec(AVStream *stream, struct lms_string_size *value)
{
    switch (stream->codec->profile) {
    case FF_PROFILE_AAC_MAIN:
        lms_string_size_dup(value, &_codec_audio_mpeg4aac_main);
        break;
    case FF_PROFILE_AAC_LOW:
        lms_string_size_dup(value, &_codec_audio_mpeg4aac_lc);
        break;
    case FF_PROFILE_AAC_SSR:
        lms_string_size_dup(value, &_codec_audio_mpeg4aac_ssr);
        break;
    case FF_PROFILE_AAC_LTP:
        lms_string_size_dup(value, &_codec_audio_mpeg4aac_ltp);
        break;
    case FF_PROFILE_AAC_HE:
        lms_string_size_dup(value, &_codec_audio_mpeg4aac_he);
        break;
    default:
        lms_string_size_dup(value, &_codec_audio_mpeg4aac_scalable);
        break;
    }
}

/** TODO: for mp4 we're parsing a smaller subset of codec than mp4 plugin itself */
static void
_mp4_get_video_codec(AVStream *stream, struct lms_string_size *value)
{
    struct lms_string_size ret = {};
    char str_profile[64], str_level[64], buf[256];
    int level, profile;

    profile = stream->codec->profile;
    level = stream->codec->level;

    switch (profile) {
    case 66:
        memcpy(str_profile, "baseline", sizeof("baseline"));
        break;
    case 77:
        memcpy(str_profile, "main", sizeof("main"));
        break;
    case 88:
        memcpy(str_profile, "extended", sizeof("extended"));
        break;
    case 100:
        memcpy(str_profile, "high", sizeof("high"));
        break;
    case 110:
        memcpy(str_profile, "high-10", sizeof("high-10"));
        break;
    case 122:
        memcpy(str_profile, "high-422", sizeof("high-422"));
        break;
    case 144:
        memcpy(str_profile, "high-444", sizeof("high-444"));
        break;
    default:
        snprintf(str_profile, sizeof(str_profile), "unknown-%d", profile);
    }

    if (level % 10 == 0)
        snprintf(str_level, sizeof(str_level), "%u", level / 10);
    else
        snprintf(str_level, sizeof(str_level), "%u.%u", level / 10, level % 10);

    ret.len = snprintf(buf, sizeof(buf), "h264-p%s-l%s", str_profile, str_level);
    ret.str = buf;

    lms_string_size_dup(value, &ret);
}

static void
_mpeg2_get_video_codec(AVStream *stream, struct lms_string_size *value)
{
    const char *str_profile, *str_level;
    const AVCodecDescriptor *codec;
    char buf[256];

    codec = avcodec_descriptor_get(stream->codec->codec_id);
    if (!codec || !codec->name) return;

    str_profile = NULL;
    str_level = NULL;

    switch (stream->codec->profile) {
    case 6:
        str_profile = "simple";
        break;
    case 4:
        str_profile = "main";
        break;
    }

    switch (stream->codec->level) {
    case 5:
    case 8:
        str_level = "main";
        break;
    case 2:
    case 4:
        str_level = "high";
        break;
    case 6:
        str_level = "high";
        break;
    }

    snprintf(buf, sizeof(buf), "%s-p%s-l%s", codec->name, str_profile,
             str_level);
    lms_string_size_strndup(value, buf, -1);
}

static void
_get_common_codec(AVStream *stream, struct lms_string_size *value)
{
    int length, i;

    length = sizeof(_codec_list) / sizeof(struct codec_container_descriptor);
    for (i = 0; i < length; i++) {
        const struct codec_container_descriptor *curr = _codec_list + i;
        if (curr->id == stream->codec->codec_id) {
            lms_string_size_dup(value, curr->desc);
            break;
        }
    }
}

static void
_get_common_container(AVStream *stream, struct lms_string_size *value)
{
    int length, i;

    length = sizeof(_container_list) / sizeof(struct codec_container_descriptor);
    for (i = 0; i < length; i++) {
        const struct codec_container_descriptor *curr = _container_list + i;
        if (curr->id == stream->codec->codec_id) {
            lms_string_size_dup(value, curr->desc);
            break;
        }
    }
}

static const struct codec_container _codecs[] = {
    {
        .id = AV_CODEC_ID_MP3,
        .get_codec = _get_common_codec,
        .get_container = NULL,
    },
    {
        .id = AV_CODEC_ID_VORBIS,
        .get_codec = _get_common_codec,
        .get_container = _get_common_container,
    },
    {
        .id = AV_CODEC_ID_THEORA,
        .get_codec = _get_common_codec,
        .get_container = _get_common_container,
    },
    {
        .id = AV_CODEC_ID_AAC,
        .get_codec = _mp4_get_audio_codec,
        .get_container = _get_common_container,
    },
    {
        .id = AV_CODEC_ID_MPEG2VIDEO,
        .get_codec = _mpeg2_get_video_codec,
        .get_container = NULL,
    },
    {
        .id = AV_CODEC_ID_MPEG2VIDEO,
        .get_codec = _mp4_get_video_codec,
        .get_container = _get_common_container,
    },
    {
        .id = AV_CODEC_ID_H264,
        .get_codec = _mp4_get_video_codec,
        .get_container = _get_common_container,
    },
    {
        .id = AV_CODEC_ID_WMAV1,
        .get_codec = _get_common_codec,
        .get_container = _get_common_container,
    },
    {
        .id = AV_CODEC_ID_WMAV2,
        .get_codec = _get_common_codec,
        .get_container = _get_common_container,
    },
    {
        .id = AV_CODEC_ID_WMAPRO,
        .get_codec = _get_common_codec,
        .get_container = _get_common_container,
    },
};

static const char *_cats[] = {
    "multimedia",
    "audio",
    "video",
    NULL
};

static const char *_authors[] = {
    "Leandro Dorileo",
    NULL
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
    lms_db_video_t *video_db;
};

struct mpeg_info {
    struct lms_string_size title;
    struct lms_string_size artist;
    struct lms_string_size album;
    struct lms_string_size genre;
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

static char *
_get_dict_value(AVDictionary *dict, const char *key)
{
    AVDictionaryEntry *tag = NULL;
    tag = av_dict_get(dict, key, tag, AV_DICT_IGNORE_SUFFIX);
    if (!tag) return NULL;
    return tag->value;
}

static const struct codec_container *
_find_codec_container(unsigned int codec_id)
{
    int i, length;
    length = sizeof(_codecs) / sizeof(struct codec_container);

    for (i = 0; i < length; i++) {
      const struct codec_container *curr = &_codecs[i];
      if (curr->id == codec_id) return curr;
    }

    return NULL;
}

static void
_get_codec(AVStream *stream, struct lms_string_size *value)
{
    const struct codec_container *cc;

    cc = _find_codec_container(stream->codec->codec_id);
    if (!cc || !cc->get_codec) return;

    cc->get_codec(stream, value);
}

static void
_get_container(AVStream *stream, struct lms_string_size *value)
{
    const struct codec_container *cc;

    cc = _find_codec_container(stream->codec->codec_id);
    if (!cc || !cc->get_container) return;

    cc->get_container(stream, value);
}

static int
_get_stream_duration(AVFormatContext *fmt_ctx)
{
    int64_t duration;
    if (fmt_ctx->duration == AV_NOPTS_VALUE) return 0;

    duration = fmt_ctx->duration + 5000;
    return (duration / AV_TIME_BASE);
}

static void
_parse_audio_stream(AVFormatContext *fmt_ctx, struct lms_audio_info *info, AVStream *stream)
{
    AVCodecContext *ctx = stream->codec;

    info->bitrate = ctx->bit_rate;
    info->channels = ctx->channels;

    if (!info->channels)
        info->channels = av_get_channel_layout_nb_channels(ctx->channel_layout);

    info->sampling_rate = ctx->sample_rate;
    info->length = _get_stream_duration(fmt_ctx);

    _get_codec(stream, &info->codec);
}

static void
_parse_video_stream(AVFormatContext *fmt_ctx, struct lms_video_info *info, AVStream *stream, char *language)
{
    char aspect_ratio[256];
    struct lms_stream *s;
    AVCodecContext *ctx = stream->codec;

    s = calloc(1, sizeof(*s));
    if (!s) return;

    s->stream_id = (unsigned int)stream->id;
    lms_string_size_strndup(&s->lang, language, -1);

    s->type = LMS_STREAM_TYPE_VIDEO;

    _get_codec(stream, &s->codec);

    s->video.bitrate = ctx->bit_rate;
    if (!s->video.bitrate)
        s->video.bitrate = ctx->rc_max_rate;

    s->video.width = ctx->width;
    s->video.height = ctx->height;

    if (stream->avg_frame_rate.den)
        s->video.framerate = stream->avg_frame_rate.num / stream->avg_frame_rate.den;

    snprintf(aspect_ratio, sizeof(aspect_ratio), "%d:%d",
             ctx->sample_aspect_ratio.num, ctx->sample_aspect_ratio.den);

    lms_string_size_strndup(&s->video.aspect_ratio, aspect_ratio, -1);

    s->next = info->streams;
    info->streams = s;
    info->length = _get_stream_duration(fmt_ctx);
}

static int
_parse(struct plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    int ret;
    int64_t packet_size = 0;
    unsigned int i;
    AVFormatContext *fmt_ctx = NULL;
    struct mpeg_info info = { };
    char *metadata, *language;
    struct lms_audio_info audio_info = { };
    struct lms_video_info video_info = { };
    struct lms_string_size container = { };
    bool video = false;
    const struct lms_dlna_video_profile *video_dlna;
    const struct lms_dlna_audio_profile *audio_dlna;

    if (finfo->parsed)
        return 0;

    if ((ret = avformat_open_input(&fmt_ctx, finfo->path, NULL, NULL)))
        return ret;

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
        goto fail;

    metadata = _get_dict_value(fmt_ctx->metadata, "title");
    if (metadata)
        lms_string_size_strndup(&info.title, metadata, -1);

    metadata = _get_dict_value(fmt_ctx->metadata, "artist");
    if (metadata)
        lms_string_size_strndup(&info.artist, metadata, -1);

    metadata = _get_dict_value(fmt_ctx->metadata, "album");
    if (metadata)
        lms_string_size_strndup(&info.album, metadata, -1);

    metadata = _get_dict_value(fmt_ctx->metadata, "genre");
    if (metadata)
        lms_string_size_strndup(&info.genre, metadata, -1);

    av_opt_get_int(fmt_ctx, "ts_packetsize", AV_OPT_SEARCH_CHILDREN,
                   &packet_size);

    language = _get_dict_value(fmt_ctx->metadata, "language");

    for (i = 0; i < fmt_ctx->nb_streams; i++) {
        AVStream *stream = fmt_ctx->streams[i];
        AVCodecContext *ctx = stream->codec;
        AVCodec *codec;

        if (ctx->codec_id == AV_CODEC_ID_PROBE) {
            printf("Failed to probe codec for input stream %d\n", stream->index);
            continue;
        }
        
        if (!(codec = avcodec_find_decoder(ctx->codec_id))) {
            printf("Unsupported codec with id %d for input stream %d\n",
                   stream->codec->codec_id, stream->index);
            continue;
        }

        _get_container(stream, &container);

        if (ctx->codec_type == AVMEDIA_TYPE_AUDIO)
            _parse_audio_stream(fmt_ctx, &audio_info, stream);
        else if (ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            _parse_video_stream(fmt_ctx, &video_info, stream, language);
            video = true;
        }
    }

    lms_string_size_strip_and_free(&info.title);
    lms_string_size_strip_and_free(&info.artist);
    lms_string_size_strip_and_free(&info.album);
    lms_string_size_strip_and_free(&info.genre);

    if (!info.title.str)
        lms_name_from_path(&info.title, finfo->path, finfo->path_len,
                           finfo->base, _exts[((long) match) - 1].len,
                           NULL);

    if (info.title.str)
        lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);
    if (info.artist.str)
        lms_charset_conv(ctxt->cs_conv, &info.artist.str, &info.artist.len);
    if (info.album.str)
      lms_charset_conv(ctxt->cs_conv, &info.album.str, &info.album.len);
    if (info.genre.str)
        lms_charset_conv(ctxt->cs_conv, &info.genre.str, &info.genre.len);


    if (!container.str)
        lms_string_size_strndup(&container, fmt_ctx->iformat->name, -1);

    if (video) {
        video_info.id = finfo->id;
        video_info.title = info.title;
        video_info.artist = info.artist;
        video_info.container = container;
        video_info.packet_size = packet_size;

        LMS_DLNA_GET_VIDEO_PROFILE_PATH_FB(&video_info, video_dlna,
                                           finfo->path);
        ret = lms_db_video_add(plugin->video_db, &video_info);
    } else {
        audio_info.id = finfo->id;
        audio_info.title = info.title;
        audio_info.artist = info.artist;
        audio_info.album = info.album;
        audio_info.genre = info.genre;
        audio_info.container = container;

        LMS_DLNA_GET_AUDIO_PROFILE_PATH_FB(&audio_info, audio_dlna,
                                           finfo->path);
        ret = lms_db_audio_add(plugin->audio_db, &audio_info);

        lms_string_size_strip_and_free(&audio_info.codec);
    }

    free(info.title.str);
    free(info.artist.str);
    free(info.album.str);
    free(info.genre.str);
    free(container.str);

    while (video_info.streams) {
        struct lms_stream *s = video_info.streams;
        video_info.streams = s->next;
        free(s->codec.str);
        if (s->type == LMS_STREAM_TYPE_VIDEO) {
            free(s->video.aspect_ratio.str);
        }
        free(s->lang.str);
        free(s);
    }

 fail:
    avformat_close_input(&fmt_ctx);
    return ret;
}

static int
_close(struct plugin *plugin)
{
    free(plugin);
    return 0;
}

static int
_setup(struct plugin *plugin,  struct lms_context *ctxt)
{
    av_register_all();
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
    plugin->plugin.order = INT32_MAX;

    return (struct lms_plugin *)plugin;
}

API const struct lms_plugin_info *
lms_plugin_info(void)
{
    static struct lms_plugin_info info = {
        _name,
        _cats,
        "libavcodec",
        PACKAGE_VERSION,
        _authors,
        "http://github.com/profusion/lightmediascanner"
    };

    return &info;
}
