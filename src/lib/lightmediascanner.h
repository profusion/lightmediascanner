/**
 * Copyright (C) 2008 by ProFUSION embedded systems
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
 * @author Gustavo Sverzut Barbieri <barbieri@profusion.mobi>
 * @author Gustavo Sverzut Barbieri <gustavo.barbieri@openbossa.org>
 */

/**
 * @mainpage
 *
 * The architecture is based on 2 processes that cooperate, the first is
 * the driver, that controls the behavior of the worker/slave process,
 * that does the hard work. This slave process is meant to make the
 * software more robust since some parser libraries and even
 * user-provided media is not reliable, so if for some reason the worker
 * process freezes, it's killed and then restarted with the next item.
 *
 * User API is quite simple, with means to add new charsets to be tried
 * and new parsers to handle media. The most important functions are (see
 * lightmediascanner.h):
 *
 *   - int lms_process(lms_t *lms, const char *top_path)
 *   - int lms_check(lms_t *lms, const char *top_path)
 *
 * @note The whole library follows libC standard of "0 (zero) means success",
 * unless explicitly stated (usually boolean queries where no error is
 * possible/interesting).
 *
 * The first will walk all the files and children directories of
 * top_path, check if files are handled by some parser and if they're,
 * they'll be parsed and registered in the data base.
 *
 * The second will get all already registered media in data base that is
 * located at top_path and see if they're still up to date, deleted or
 * changed. If they were deleted, a flag is set on data base with current
 * time, so it can be expired at some point. If they were marked as
 * deleted, but are not present again, check if the state is still valid
 * (mtime, size), so we can avoid re-parse of removable media. If the
 * file was present and is still present, just check if its properties
 * (mtime, size) are still the same, if not trigger re-parse.
 *
 * Parsers are handled as shared object plugins, they can be added
 * without modification to the core, see the plugins API later in this
 * document. Since the core have no control over plugins, they can
 * register data as they want, but since some utilities are provided, we
 * expect that the given data base tables are used:
 *
 *   - @b files: known files.
 *      - id: identification inside LMS/DB.
 *      - path: file path.
 *      - mtime: modification time, in seconds from UNIX epoch.
 *      - dtime: modification time, in seconds from UNIX epoch.
 *      - size: in bytes.
 *   - @b audios: audio files.
 *      - id: same as files.id
 *      - title: audio title.
 *      - album_id: same as audio_albums.id.
 *      - genre_id: same as audio_genres.id.
 *      - trackno: track number.
 *      - rating: rating.
 *      - playcnt: play count.
 *   - @b audio_artists: audio artists.
 *      - id: identification inside LMS/DB.
 *      - name: artist name.
 *   - @b audio_albums: audio albums.
 *      - id: identification inside LMS/DB.
 *      - artist_id: same as audio_artists.id.
 *      - name: album name.
 *   - @b audio_genres: audio genres.
 *      - id: identification inside LMS/DB.
 *      - name: genre name.
 *   - @b playlists: playlists.
 *      - id: identification inside LMS/DB.
 *      - title: playlists title.
 *      - n_entries: number of entries in this playlist.
 *   - @b images: image files.
 *      - id: identification inside LMS/DB.
 *      - title: image title.
 *      - artist: image creator or artirst or photographer or ...
 *      - date: image taken date or creation date or ...
 *      - width: image width.
 *      - height: image height.
 *      - orientation: image orientation.
 *      - gps_lat: GPS latitude.
 *      - gps_long: GPS longitude.
 *      - gps_alt: GPS altitude.
 *   - @b videos: video files.
 *      - id: identification inside LMS/DB.
 *      - title: video title.
 *      - artist: video artist or creator or producer or ...
 */


#ifndef _LIGHTMEDIASCANNER_H_
#define _LIGHTMEDIASCANNER_H_ 1

#ifdef API
#undef API
#endif

#ifdef __GNUC__
# if __GNUC__ >= 4
#  define API __attribute__ ((visibility("default")))
#  define GNUC_NULL_TERMINATED __attribute__((__sentinel__))
# else
#  define API
#  define GNUC_NULL_TERMINATED
# endif
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
#  define GNUC_PURE __attribute__((__pure__))
#  define GNUC_MALLOC __attribute__((__malloc__))
#  define GNUC_CONST __attribute__((__const__))
#  define GNUC_UNUSED __attribute__((__unused__))
# else
#  define GNUC_PURE
#  define GNUC_MALLOC
#  define GNUC_NORETURN
#  define GNUC_CONST
#  define GNUC_UNUSED
# endif
# if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#  define GNUC_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#  define GNUC_NON_NULL(...) __attribute__((nonnull(__VA_ARGS__)))
# else
#  define GNUC_WARN_UNUSED_RESULT
#  define GNUC_NON_NULL(...)
# endif
#else
#  define API
#  define GNUC_NULL_TERMINATED
#  define GNUC_PURE
#  define GNUC_MALLOC
#  define GNUC_CONST
#  define GNUC_UNUSED
#  define GNUC_WARN_UNUSED_RESULT
#  define GNUC_NON_NULL(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif
    /**
     * @defgroup LMS_API User-API
     *
     * Functions for library users.
     */

    typedef struct lms lms_t;
    typedef struct lms_plugin lms_plugin_t;

    typedef enum {
        LMS_PROGRESS_STATUS_UP_TO_DATE,
        LMS_PROGRESS_STATUS_PROCESSED,
        LMS_PROGRESS_STATUS_DELETED,
        LMS_PROGRESS_STATUS_KILLED,
        LMS_PROGRESS_STATUS_ERROR_PARSE,
        LMS_PROGRESS_STATUS_ERROR_COMM,
    } lms_progress_status_t;

    typedef void (*lms_free_callback_t)(void *data);
    typedef void (*lms_progress_callback_t)(lms_t *lms, const char *path, int path_len, lms_progress_status_t status, void *data);

    API lms_t *lms_new(const char *db_path) GNUC_MALLOC GNUC_WARN_UNUSED_RESULT;
    API int lms_free(lms_t *lms) GNUC_NON_NULL(1);
    API int lms_process(lms_t *lms, const char *top_path) GNUC_NON_NULL(1, 2);
    API int lms_process_single_process(lms_t *lms, const char *top_path) GNUC_NON_NULL(1, 2);
    API int lms_check(lms_t *lms, const char *top_path) GNUC_NON_NULL(1, 2);
    API void lms_stop_processing(lms_t *lms) GNUC_NON_NULL(1);
    API const char *lms_get_db_path(const lms_t *lms) GNUC_NON_NULL(1);
    API int lms_is_processing(const lms_t *lms) GNUC_PURE GNUC_NON_NULL(1);
    API int lms_get_slave_timeout(const lms_t *lms) GNUC_NON_NULL(1);
    API void lms_set_slave_timeout(lms_t *lms, int ms) GNUC_NON_NULL(1);
    API unsigned int lms_get_commit_interval(const lms_t *lms) GNUC_NON_NULL(1);
    API void lms_set_commit_interval(lms_t *lms, unsigned int transactions) GNUC_NON_NULL(1);
    API void lms_set_progress_callback(lms_t *lms, lms_progress_callback_t cb, const void *data, lms_free_callback_t free_data) GNUC_NON_NULL(1);

    API lms_plugin_t *lms_parser_add(lms_t *lms, const char *so_path) GNUC_NON_NULL(1, 2);
    API lms_plugin_t *lms_parser_find_and_add(lms_t *lms, const char *name) GNUC_NON_NULL(1, 2);
    API int lms_parser_del(lms_t *lms, lms_plugin_t *handle) GNUC_NON_NULL(1, 2);

    API int lms_charset_add(lms_t *lms, const char *charset) GNUC_NON_NULL(1, 2);
    API int lms_charset_del(lms_t *lms, const char *charset) GNUC_NON_NULL(1, 2);

#ifdef __cplusplus
}
#endif
#endif /* _LIGHTMEDIASCANNER_H_ */
