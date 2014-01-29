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

#ifndef _LIGHTMEDIASCANNER_DB_H_
#define _LIGHTMEDIASCANNER_DB_H_ 1

#ifdef API
#undef API
#endif

#ifdef __GNUC__
# if __GNUC__ >= 4
#  define API __attribute__ ((visibility("default")))
# else
#  define API
# endif
# if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#  define GNUC_NON_NULL(...) __attribute__((nonnull(__VA_ARGS__)))
# else
#  define GNUC_NON_NULL(...)
# endif
#else
#  define API
#  define GNUC_NON_NULL(...)
#endif

#include <lightmediascanner_plugin.h>
#include <lightmediascanner_utils.h>
#include <inttypes.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @defgroup LMS_DB DataBase-API
 *
 * Although Light Media Scanner uses SQLite3 and doesn't try to hide it from
 * plugins/parsers, it does provide some utilities to make development easier
 * and less error prone.
 *
 * @warning This is to be considered an *internal* API for plugins/parsers and
 * there's no versioning or backward compatibility whatsoever. It's much
 * better to keep plugins inside LMS tree than keeping them separate.
 *
 * @{
 */

    /* Image Records */
    struct lms_gps_info {
        double latitude;
        double longitude;
        double altitude;
    };

    struct lms_image_info {
        int64_t id;
        struct lms_string_size title;
        struct lms_string_size artist;
        struct lms_string_size dlna_profile;
        struct lms_string_size dlna_mime;
        unsigned int date;
        unsigned short width;
        unsigned short height;
        unsigned short orientation;
        struct lms_gps_info gps;
        struct lms_string_size container;
    };

    typedef struct lms_db_image lms_db_image_t;

    API lms_db_image_t *lms_db_image_new(sqlite3 *db) GNUC_NON_NULL(1);
    API int lms_db_image_start(lms_db_image_t *ldi) GNUC_NON_NULL(1);
    API int lms_db_image_free(lms_db_image_t *ldi) GNUC_NON_NULL(1);
    API int lms_db_image_add(lms_db_image_t *ldi, struct lms_image_info *info) GNUC_NON_NULL(1, 2);

    /* Audio Records */
    struct lms_audio_info {
        int64_t id;
        struct lms_string_size title;
        struct lms_string_size artist;
        struct lms_string_size album;
        struct lms_string_size genre;
        struct lms_string_size container;
        struct lms_string_size codec;
        struct lms_string_size dlna_profile;
        struct lms_string_size dlna_mime;
        unsigned int playcnt;
        unsigned int length;
        unsigned int sampling_rate;
        unsigned int bitrate;
        uint8_t trackno;
        uint8_t rating;
        uint8_t channels;
    };

    typedef struct lms_db_audio lms_db_audio_t;

    API lms_db_audio_t *lms_db_audio_new(sqlite3 *db) GNUC_NON_NULL(1);
    API int lms_db_audio_start(lms_db_audio_t *lda) GNUC_NON_NULL(1);
    API int lms_db_audio_free(lms_db_audio_t *lda) GNUC_NON_NULL(1);
    API int lms_db_audio_add(lms_db_audio_t *lda, struct lms_audio_info *info) GNUC_NON_NULL(1, 2);

    /* Video Records */

    enum lms_stream_type {
        LMS_STREAM_TYPE_UNKNOWN,
        LMS_STREAM_TYPE_AUDIO,
        LMS_STREAM_TYPE_VIDEO,
        LMS_STREAM_TYPE_SUBTITLE,
    };

    struct lms_stream_video_info {
        struct lms_string_size aspect_ratio;
        unsigned int bitrate;
        unsigned int width;
        unsigned int height;
        double framerate;
        bool interlaced;
    };

    struct lms_stream_audio_info {
        unsigned int sampling_rate;
        unsigned int bitrate;
        uint8_t channels;
    };

    struct lms_stream {
        struct lms_stream *next;
        enum lms_stream_type type;
        unsigned int stream_id;
        struct lms_string_size codec;
        struct lms_string_size lang;
        union {
            struct lms_stream_audio_info audio;
            struct lms_stream_video_info video;
        };
    };

    struct lms_video_info {
        int64_t id;
        struct lms_string_size title;
        struct lms_string_size artist;
        struct lms_string_size container;
        struct lms_string_size dlna_profile;
        struct lms_string_size dlna_mime;
        unsigned int length;
        int64_t packet_size;
        struct lms_stream *streams;
    };

    typedef struct lms_db_video lms_db_video_t;

    API lms_db_video_t *lms_db_video_new(sqlite3 *db) GNUC_NON_NULL(1);
    API int lms_db_video_start(lms_db_video_t *ldv) GNUC_NON_NULL(1);
    API int lms_db_video_free(lms_db_video_t *ldv) GNUC_NON_NULL(1);
    API int lms_db_video_add(lms_db_video_t *ldv, struct lms_video_info *info) GNUC_NON_NULL(1, 2);

    API int lms_stream_video_info_aspect_ratio_guess(struct lms_stream_video_info *info) GNUC_NON_NULL(1);

    /* Playlist Records */
    struct lms_playlist_info {
        int64_t id;
        struct lms_string_size title;
        unsigned int n_entries;
    };

    typedef struct lms_db_playlist lms_db_playlist_t;

    API lms_db_playlist_t *lms_db_playlist_new(sqlite3 *db) GNUC_NON_NULL(1);
    API int lms_db_playlist_start(lms_db_playlist_t *ldp) GNUC_NON_NULL(1);
    API int lms_db_playlist_free(lms_db_playlist_t *ldp) GNUC_NON_NULL(1);
    API int lms_db_playlist_add(lms_db_playlist_t *ldp, struct lms_playlist_info *info) GNUC_NON_NULL(1, 2);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif /* _LIGHTMEDIASCANNER_DB_H_ */
