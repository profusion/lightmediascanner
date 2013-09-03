/**
 * Copyright (C) 2008 by INdT
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
 * @author Andre Moreira Magalhaes <andre.magalhaes@openbossa.org>
 */

/**
 * @brief
 *
 * mp4 file parser.
 */

static const char PV[] = PACKAGE_VERSION; /* mp4.h screws PACKAGE_VERSION */

#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>
#include <shared/util.h>

#ifdef HAVE_MP4V2
#include <mp4v2/mp4v2.h>
#else
#include <mp4.h>
#endif
#include <string.h>
#include <stdlib.h>

struct mp4_info {
    struct lms_string_size title;
    struct lms_string_size artist;
    struct lms_string_size album;
    struct lms_string_size genre;
    uint16_t trackno;
    uint64_t length;
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
    lms_db_video_t *video_db;
};

static const char _name[] = "mp4";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".mp4"),
    LMS_STATIC_STRING_SIZE(".m4a"),
    LMS_STATIC_STRING_SIZE(".m4v"),
    LMS_STATIC_STRING_SIZE(".mov"),
    LMS_STATIC_STRING_SIZE(".qt"),
    LMS_STATIC_STRING_SIZE(".3gp")
};
static const char *_cats[] = {
    "multimedia",
    "audio",
    "video",
    NULL
};
static const char *_authors[] = {
    "Andre Moreira Magalhaes",
    NULL
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

#ifdef HAVE_MP4V2_2_0_API
static int
_parse(struct plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    struct mp4_info info = { };
    struct lms_audio_info audio_info = { };
    struct lms_video_info video_info = { };
    int r, stream_type = LMS_STREAM_TYPE_AUDIO;
    MP4FileHandle mp4_fh;
    u_int32_t num_tracks, i;
    const MP4Tags *tags;

    mp4_fh = MP4Read(finfo->path);
    if (mp4_fh == MP4_INVALID_FILE_HANDLE) {
        fprintf(stderr, "ERROR: cannot read mp4 file %s\n", finfo->path);
        return -1;
    }

    tags = MP4TagsAlloc();
    if (!tags)
        return -1;

    if (!MP4TagsFetch(tags, mp4_fh)) {
        r = -1;
        goto fail;
    }

#define STR_FIELD_FROM_TAG(_tags_field, _field)                         \
    do {                                                                \
        if (_tags_field) {                                              \
            _field.len = strlen(_tags_field);                           \
            _field.str = malloc(_field.len);                            \
            memcpy(_field.str, _tags_field, _field.len + 1);            \
        }                                                               \
    } while (0)

    STR_FIELD_FROM_TAG(tags->name, info.title);
    STR_FIELD_FROM_TAG(tags->artist, info.artist);

    /* check if the file contains a video track */
    num_tracks = MP4GetNumberOfTracks(mp4_fh, MP4_VIDEO_TRACK_TYPE, 0);
    if (num_tracks > 0)
        stream_type = LMS_STREAM_TYPE_VIDEO;

    info.length = MP4GetDuration(mp4_fh) /
        MP4GetTimeScale(mp4_fh) ?: 1;

    if (stream_type == LMS_STREAM_TYPE_AUDIO) {
        MP4TrackId id;

        STR_FIELD_FROM_TAG(tags->album, info.album);
        STR_FIELD_FROM_TAG(tags->genre, info.genre);
        if (tags->track)
            info.trackno = tags->track->index;

        id = MP4FindTrackId(mp4_fh, 0, MP4_AUDIO_TRACK_TYPE, 0);
        audio_info.bitrate = MP4GetTrackBitRate(mp4_fh, id);
        audio_info.channels = MP4GetTrackAudioChannels(mp4_fh, id);
        audio_info.sampling_rate = MP4GetTrackTimeScale(mp4_fh, id);
        audio_info.length = info.length;
    } else {
        num_tracks = MP4GetNumberOfTracks(mp4_fh, NULL, 0);
        for (i = 0; i < num_tracks; i++) {
            MP4TrackId id = MP4FindTrackId(mp4_fh, i, NULL, 0);
            const char *type = MP4GetTrackType(mp4_fh, id);
            enum lms_stream_type lmstype;
            struct lms_stream *s;

            if (strcmp(type, MP4_AUDIO_TRACK_TYPE) == 0)
                lmstype = LMS_STREAM_TYPE_AUDIO;
            else if (strcmp(type, MP4_VIDEO_TRACK_TYPE) == 0)
                lmstype = LMS_STREAM_TYPE_VIDEO;
            else
                continue;

            s = calloc(1, sizeof(*s));
            s->type = lmstype;
            s->stream_id = id;

            if (lmstype == LMS_STREAM_TYPE_AUDIO) {
                s->audio.channels = MP4GetTrackAudioChannels(mp4_fh, id);
                s->audio.bitrate = MP4GetTrackBitRate(mp4_fh, id);
            } else if (lmstype == LMS_STREAM_TYPE_VIDEO) {
                s->video.width = MP4GetTrackVideoWidth(mp4_fh, id);
                s->video.height = MP4GetTrackVideoHeight(mp4_fh, id);
                s->video.framerate = MP4GetTrackVideoFrameRate(mp4_fh, id);
            }

            s->next = video_info.streams;
            video_info.streams = s;
        }
        video_info.length = info.length;
    }
#undef STR_FIELD_FROM_TAG

    lms_string_size_strip_and_free(&info.title);
    lms_string_size_strip_and_free(&info.artist);
    lms_string_size_strip_and_free(&info.album);
    lms_string_size_strip_and_free(&info.genre);

    if (!info.title.str)
        info.title = str_extract_name_from_path(finfo->path, finfo->path_len,
                                                finfo->base,
                                                &_exts[((long) match) - 1],
                                                NULL);
    if (info.title.str)
        lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);
    if (info.artist.str)
        lms_charset_conv(ctxt->cs_conv, &info.artist.str, &info.artist.len);
    if (info.album.str)
        lms_charset_conv(ctxt->cs_conv, &info.album.str, &info.album.len);
    if (info.genre.str)
        lms_charset_conv(ctxt->cs_conv, &info.genre.str, &info.genre.len);

    if (stream_type == LMS_STREAM_TYPE_AUDIO) {
        audio_info.id = finfo->id;
        audio_info.title = info.title;
        audio_info.artist = info.artist;
        audio_info.album = info.album;
        audio_info.genre = info.genre;
        audio_info.trackno = info.trackno;
        r = lms_db_audio_add(plugin->audio_db, &audio_info);
    } else {
        video_info.id = finfo->id;
        video_info.title = info.title;
        video_info.artist = info.artist;
        r = lms_db_video_add(plugin->video_db, &video_info);
    }

fail:
    MP4TagsFree(tags);

    free(info.title.str);
    free(info.artist.str);
    free(info.album.str);
    free(info.genre.str);

    while (video_info.streams) {
        struct lms_stream *s = video_info.streams;
        video_info.streams = s->next;
        free(s);
    }

    MP4Close(mp4_fh, 0);

    return r;
}
#else
static int
_parse(struct plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    struct mp4_info info = { };
    struct lms_audio_info audio_info = { };
    struct lms_video_info video_info = { };
    int r, stream_type = LMS_STREAM_TYPE_AUDIO;
    MP4FileHandle mp4_fh;
    u_int32_t num_tracks;

    mp4_fh = MP4Read(finfo->path, 0);
    if (mp4_fh == MP4_INVALID_FILE_HANDLE) {
        fprintf(stderr, "ERROR: cannot read mp4 file %s\n", finfo->path);
        return -1;
    }

    /* check if the file contains a video track */
    num_tracks = MP4GetNumberOfTracks(mp4_fh, MP4_VIDEO_TRACK_TYPE, 0);
    if (num_tracks > 0)
        stream_type = LMS_STREAM_TYPE_VIDEO;

    MP4GetMetadataName(mp4_fh, &info.title.str);
    if (info.title.str)
        info.title.len = strlen(info.title.str);
    MP4GetMetadataArtist(mp4_fh, &info.artist.str);
    if (info.artist.str)
        info.artist.len = strlen(info.artist.str);

    if (stream_type == LMS_STREAM_TYPE_AUDIO) {
        u_int16_t total_tracks;

        MP4GetMetadataAlbum(mp4_fh, &info.album.str);
        if (info.album.str)
            info.album.len = strlen(info.album.str);
        MP4GetMetadataGenre(mp4_fh, &info.genre.str);
        if (info.genre.str)
            info.genre.len = strlen(info.genre.str);

        MP4GetMetadataTrack(mp4_fh, &info.trackno, &total_tracks);
    }

    lms_string_size_strip_and_free(&info.title);
    lms_string_size_strip_and_free(&info.artist);
    lms_string_size_strip_and_free(&info.album);
    lms_string_size_strip_and_free(&info.genre);

    if (!info.title.str)
        info.title = str_extract_name_from_path(finfo->path, finfo->path_len,
                                                finfo->base,
                                                &_exts[((long) match) - 1],
                                                NULL);
    if (info.title.str)
        lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);

    if (info.artist.str)
        lms_charset_conv(ctxt->cs_conv, &info.artist.str, &info.artist.len);
    if (info.album.str)
        lms_charset_conv(ctxt->cs_conv, &info.album.str, &info.album.len);
    if (info.genre.str)
        lms_charset_conv(ctxt->cs_conv, &info.genre.str, &info.genre.len);

#if 0
    fprintf(stderr, "file %s info\n", finfo->path);
    fprintf(stderr, "\ttitle='%s'\n", info.title.str);
    fprintf(stderr, "\tartist='%s'\n", info.artist.str);
    fprintf(stderr, "\talbum='%s'\n", info.album.str);
    fprintf(stderr, "\tgenre='%s'\n", info.genre.str);
#endif

    if (stream_type == LMS_STREAM_TYPE_AUDIO) {
        audio_info.id = finfo->id;
        audio_info.title = info.title;
        audio_info.artist = info.artist;
        audio_info.album = info.album;
        audio_info.genre = info.genre;
        audio_info.trackno = info.trackno;
        r = lms_db_audio_add(plugin->audio_db, &audio_info);
    }
    else {
        video_info.id = finfo->id;
        video_info.title = info.title;
        video_info.artist = info.artist;
        r = lms_db_video_add(plugin->video_db, &video_info);
    }

    MP4Close(mp4_fh);

    free(info.title.str);
    free(info.artist.str);
    free(info.album.str);
    free(info.genre.str);

    return r;
}
#endif

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
        "MP4 files (MP4, M4A, MOV, QT, 3GP)",
        PV,
        _authors,
        "http://lms.garage.maemo.org"
    };

    return &info;
}
