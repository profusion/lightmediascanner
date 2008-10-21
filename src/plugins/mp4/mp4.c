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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static const char PV[] = PACKAGE_VERSION; /* mp4.h screws PACKAGE_VERSION */

#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>
#include <mp4.h>
#include <string.h>
#include <stdlib.h>

enum StreamTypes {
    STREAM_TYPE_UNKNOWN = 0,
    STREAM_TYPE_AUDIO,
    STREAM_TYPE_VIDEO
};

struct mp4_info {
    struct lms_string_size title;
    struct lms_string_size artist;
    struct lms_string_size album;
    struct lms_string_size genre;
    u_int16_t trackno;
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
    struct mp4_info info = {{0}, {0}, {0}, {0}};
    struct lms_audio_info audio_info = {0, {0}, {0}, {0}, {0}, 0, 0, 0};
    struct lms_video_info video_info = {0, {0}, {0}};
    int r, stream_type = STREAM_TYPE_AUDIO;
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
        stream_type = STREAM_TYPE_VIDEO;

    MP4GetMetadataName(mp4_fh, &info.title.str);
    if (info.title.str)
        info.title.len = strlen(info.title.str);
    MP4GetMetadataArtist(mp4_fh, &info.artist.str);
    if (info.artist.str)
        info.artist.len = strlen(info.artist.str);

    if (stream_type == STREAM_TYPE_AUDIO) {
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

    if (!info.title.str) {
        int ext_idx;
        ext_idx = ((int)match) - 1;
        info.title.len = finfo->path_len - finfo->base - _exts[ext_idx].len;
        info.title.str = malloc((info.title.len + 1) * sizeof(char));
        memcpy(info.title.str, finfo->path + finfo->base, info.title.len);
        info.title.str[info.title.len] = '\0';
    }
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

    if (stream_type == STREAM_TYPE_AUDIO) {
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

API struct lms_plugin_info *
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
