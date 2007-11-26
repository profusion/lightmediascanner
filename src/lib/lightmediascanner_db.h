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

#ifdef __cplusplus
extern "C" {
#endif

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
        unsigned int date;
        unsigned short width;
        unsigned short height;
        unsigned short orientation;
        struct lms_gps_info gps;
    };

    typedef struct lms_db_image lms_db_image_t;

    API lms_db_image_t *lms_db_image_new(sqlite3 *db) GNUC_NON_NULL(1);
    API int lms_db_image_start(lms_db_image_t *ldi) GNUC_NON_NULL(1);
    API int lms_db_image_free(lms_db_image_t *lms_db_image) GNUC_NON_NULL(1);
    API int lms_db_image_add(lms_db_image_t *lms_db_image, struct lms_image_info *info) GNUC_NON_NULL(1, 2);

    /* Audio Records */
    struct lms_audio_info {
        int64_t id;
        struct lms_string_size title;
        struct lms_string_size artist;
        struct lms_string_size album;
        struct lms_string_size genre;
        unsigned char trackno;
        unsigned char rating;
    };

    typedef struct lms_db_audio lms_db_audio_t;

    API lms_db_audio_t *lms_db_audio_new(sqlite3 *db) GNUC_NON_NULL(1);
    API int lms_db_audio_start(lms_db_audio_t *ldi) GNUC_NON_NULL(1);
    API int lms_db_audio_free(lms_db_audio_t *lms_db_audio) GNUC_NON_NULL(1);
    API int lms_db_audio_add(lms_db_audio_t *lms_db_audio, struct lms_audio_info *info) GNUC_NON_NULL(1, 2);

#ifdef __cplusplus
}
#endif
#endif /* _LIGHTMEDIASCANNER_DB_H_ */
