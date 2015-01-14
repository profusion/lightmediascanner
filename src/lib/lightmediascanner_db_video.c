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

#include <lightmediascanner_db.h>
#include <lightmediascanner_dlna.h>
#include "lightmediascanner_db_private.h"
#include <magic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct lms_db_video {
    sqlite3 *db;
    sqlite3_stmt *insert;
    sqlite3_stmt *insert_video_streams;
    sqlite3_stmt *insert_audio_streams;
    sqlite3_stmt *insert_subtitle_streams;
    unsigned int _references;
    unsigned int _is_started:1;
    magic_t magic;
};

static struct lms_db_cache _cache = { };

static int
_db_table_updater_videos_0(sqlite3 *db, const char *table, unsigned int current_version, int is_last_run) {
    return 0;
}

static int
_db_table_updater_videos_1(sqlite3 *db, const char *table, unsigned int current_version, int is_last_run) {
    char *errmsg;
    int r, ret;

    errmsg = NULL;
    r = sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS videos ("
                     "id INTEGER PRIMARY KEY, "
                     "title TEXT, "
                     "artist TEXT, "
                     "length INTEGER"
                     ")",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create 'videos' table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    r = sqlite3_exec(db,
                     "CREATE INDEX IF NOT EXISTS videos_title_idx ON videos ("
                     "title"
                     ")",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr,
                "ERROR: could not create 'videos_title_idx' index: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -2;
    }

    r = sqlite3_exec(db,
                     "CREATE INDEX IF NOT EXISTS videos_artist_idx ON videos ("
                     "artist"
                     ")",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr,
                "ERROR: could not create 'videos_artist_idx' index: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -3;
    }

    ret = lms_db_create_trigger_if_not_exists(db,
        "delete_videos_on_files_deleted "
        "DELETE ON files FOR EACH ROW BEGIN "
        " DELETE FROM videos WHERE id = OLD.id; END;");
    if (ret != 0)
        goto done;

    ret = lms_db_create_trigger_if_not_exists(db,
        "delete_files_on_videos_deleted "
        "DELETE ON videos FOR EACH ROW BEGIN "
        " DELETE FROM files WHERE id = OLD.id; END;");

  done:
    return ret;
}

static int
_db_table_updater_videos_2(sqlite3 *db, const char *table,
                           unsigned int current_version, int is_last_run)
{
    char *errmsg = NULL;
    int r;

    r = sqlite3_exec(
        db, "BEGIN TRANSACTION;"
        "ALTER TABLE videos ADD COLUMN container TEXT DEFAULT NULL;"
        "ALTER TABLE videos ADD COLUMN dlna_profile TEXT DEFAULT NULL;"
        "ALTER TABLE videos ADD COLUMN dlna_mime TEXT DEFAULT NULL;"
        "COMMIT;",
        NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could add columns to videos table: %s\n",
                errmsg);
        sqlite3_free(errmsg);
    }

    /* Video streams */
    r = sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS videos_videos ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "video_id INTEGER, "
                     "stream_id INTEGER, "
                     "codec TEXT, "
                     "lang TEXT, "
                     "aspect_ratio TEXT, "
                     "bitrate INTEGER, "
                     "framerate REAL, "
                     "interlaced INTEGER, "
                     "width INTEGER, "
                     "height INTEGER)",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create table videos_videos\n");
        sqlite3_free(errmsg);
        return -1;
    }

    r = sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS videos_videos_video_idx "
                     "ON videos_videos (video_id)", NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr,
                "ERROR: could not create 'videos_videos_video_idx' index: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    /* Audio streams */
    r = sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS videos_audios ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "video_id INTEGER, "
                     "stream_id INTEGER, "
                     "codec TEXT, "
                     "lang TEXT, "
                     "channels INTEGER, "
                     "sampling_rate INTEGER, "
                     "bitrate INTEGER)",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create table videos_audios\n");
        sqlite3_free(errmsg);
        return -1;
    }

    r = sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS videos_audios_video_idx "
                     "ON videos_audios (video_id) ", NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr,
                "ERROR: could not create 'videos_audios_video_idx' index: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    /* Subtitle streams */
    r = sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS videos_subtitles ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "video_id INTEGER, "
                     "stream_id INTEGER, "
                     "codec TEXT, "
                     "lang TEXT)",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create table videos_subtitles\n");
        sqlite3_free(errmsg);
        return -1;
    }

    r = sqlite3_exec(db,
                     "CREATE INDEX IF NOT EXISTS videos_subtitles_video_idx "
                     "ON videos_subtitles (video_id) ", NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr,
                "ERROR: could not create 'videos_subtitles_video_idx' index: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    /* Create triggers: one for each sub-relation table, deleting the video from
     * the videos table and one to delete the sub-relation rows in case the
     * video is deleted. */

    if (lms_db_create_trigger_if_not_exists(
            db, "delete_video_on_audio_stream_deleted "
            "DELETE ON videos_audios FOR EACH ROW BEGIN "
            " DELETE FROM videos WHERE id = OLD.video_id; END;") != 0 ||
        lms_db_create_trigger_if_not_exists(
            db, "delete_video_on_video_stream_deleted "
            "DELETE ON videos_videos FOR EACH ROW BEGIN "
            " DELETE FROM videos WHERE id = OLD.video_id; END;") != 0 ||
        lms_db_create_trigger_if_not_exists(
            db, "delete_video_on_subtitle_stream_deleted "
            "DELETE ON videos_subtitles FOR EACH ROW BEGIN "
            " DELETE FROM videos WHERE id = OLD.video_id; END;") != 0) {
        fprintf(stderr ,"ERROR: could not create delete trigger\n");
        return -1;
    }

    if (lms_db_create_trigger_if_not_exists(
            db, "delete_streams_on_video_deleted "
            "DELETE ON videos FOR EACH ROW BEGIN "
            " DELETE FROM videos_videos WHERE video_id = OLD.id; "
            " DELETE FROM videos_audios WHERE video_id = OLD.id; "
            " DELETE FROM videos_subtitles WHERE video_id = OLD.id; "
            "END;") != 0)
        return -1;

    return 0;
}

static int
_db_table_updater_videos_3(sqlite3 *db, const char *table,
                           unsigned int current_version, int is_last_run)
{
    char *errmsg = NULL;
    int r;

    r = sqlite3_exec(
        db, "BEGIN TRANSACTION;"
        "ALTER TABLE videos ADD COLUMN packet_size INTEGER;"
        "COMMIT;",
        NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could add columns to videos table: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    return 0;
}

static lms_db_table_updater_t _db_table_updater_videos[] = {
    _db_table_updater_videos_0,
    _db_table_updater_videos_1,
    _db_table_updater_videos_2,
    _db_table_updater_videos_3,
};


static int
_db_create_table_if_required(sqlite3 *db)
{
    return lms_db_table_update_if_required(db, "videos",
         LMS_ARRAY_SIZE(_db_table_updater_videos),
         _db_table_updater_videos);
}

/**
 * Create video DB access tool.
 *
 * Creates or get a reference to tools to access 'videos' table in an
 * optimized and easy way.
 *
 * This is usually called from plugin's @b setup() callback with the @p db
 * got from @c ctxt.
 *
 * @param db database connection.
 *
 * @return DB access tool handle.
 * @ingroup LMS_Plugins
 */
lms_db_video_t *
lms_db_video_new(sqlite3 *db)
{
    lms_db_video_t *ldv;
    void *p;

    if (lms_db_cache_get(&_cache, db, &p) == 0) {
        ldv = p;
        ldv->_references++;
        return ldv;
    }

    if (!db)
        return NULL;

    if (_db_create_table_if_required(db) != 0) {
        fprintf(stderr, "ERROR: could not create table.\n");
        return NULL;
    }

    ldv = calloc(1, sizeof(lms_db_video_t));
    ldv->_references = 1;
    ldv->db = db;

    if (lms_db_cache_add(&_cache, db, ldv) != 0) {
        lms_db_video_free(ldv);
        return NULL;
    }

    ldv->magic = magic_open(MAGIC_MIME_TYPE);
    if (ldv->magic == NULL) {
        fprintf(stderr, "WARNING: could not initialize magic library\n");
    } else if (magic_load(ldv->magic, NULL) != 0) {
        printf("WARNING: could not load magic cache\n");
        magic_close(ldv->magic);
        ldv->magic = NULL;
    }

    return ldv;
}

/**
 * Start video DB access tool.
 *
 * Compile SQL statements and other initialization functions.
 *
 * This is usually called from plugin's @b start() callback.
 *
 * @param ldv handle returned by lms_db_video_new().
 *
 * @return On success 0 is returned.
 * @ingroup LMS_Plugins
 */
int
lms_db_video_start(lms_db_video_t *ldv)
{
    if (!ldv)
        return -1;
    if (ldv->_is_started)
        return 0;

    ldv->insert = lms_db_compile_stmt(ldv->db,
        "INSERT OR REPLACE INTO videos (id, title, artist, length, "
        "container, dlna_profile, dlna_mime, packet_size) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    if (!ldv->insert)
        return -2;

    ldv->insert_video_streams = lms_db_compile_stmt(
        ldv->db, "INSERT OR REPLACE INTO videos_videos ("
        "video_id, stream_id, codec, lang, aspect_ratio, bitrate, framerate, "
        "interlaced, width, height) VALUES ("
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    if (!ldv->insert_video_streams)
        return -1;

    ldv->insert_audio_streams = lms_db_compile_stmt(
        ldv->db, "INSERT OR REPLACE INTO videos_audios ("
        "video_id, stream_id, codec, lang, channels, sampling_rate, bitrate) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");
    if (!ldv->insert_audio_streams)
        return -1;

    ldv->insert_subtitle_streams = lms_db_compile_stmt(
        ldv->db, "INSERT OR REPLACE INTO videos_subtitles ("
        "video_id, stream_id, codec, lang) VALUES ("
        "?, ?, ?, ?)");
    if (!ldv->insert_subtitle_streams)
        return -1;

    ldv->_is_started = 1;
    return 0;
}

/**
 * Free video DB access tool.
 *
 * Unreference and possible free resources allocated to access tool.
 *
 * This is usually called from plugin's @b finish() callback.
 *
 * @param ldv handle returned by lms_db_video_new().
 *
 * @return On success 0 is returned.
 * @ingroup LMS_Plugins
 */
int
lms_db_video_free(lms_db_video_t *ldv)
{
    int r;

    if (!ldv)
        return -1;
    if (ldv->_references == 0) {
        fprintf(stderr, "ERROR: over-called lms_db_video_free(%p)\n", ldv);
        return -1;
    }

    ldv->_references--;
    if (ldv->_references > 0)
        return 0;

    if (ldv->insert)
        lms_db_finalize_stmt(ldv->insert, "insert");
    if (ldv->insert_video_streams)
        lms_db_finalize_stmt(ldv->insert_video_streams, "insert_video_streams");
    if (ldv->insert_audio_streams)
        lms_db_finalize_stmt(ldv->insert_audio_streams, "insert_audio_streams");
    if (ldv->insert_subtitle_streams)
        lms_db_finalize_stmt(ldv->insert_subtitle_streams,
                             "insert_subtitle_streams");

    r = lms_db_cache_del(&_cache, ldv->db, ldv);

    if (ldv->magic != NULL)
        magic_close(ldv->magic);

    free(ldv);

    return r;
}

static int
_db_insert_stream_subtitle(lms_db_video_t *ldv, int64_t video_id,
                           const struct lms_stream *s)
{
    sqlite3_stmt *stmt;
    int col = 0, ret;

    stmt = ldv->insert_subtitle_streams;

    if (lms_db_bind_int64(stmt, ++col, video_id) ||
        lms_db_bind_int(stmt, ++col, s->stream_id) ||
        lms_db_bind_text(stmt, ++col, s->codec.str, s->codec.len) ||
        lms_db_bind_text(stmt, ++col, s->lang.str, s->lang.len)) {
        fprintf(stderr, "ERROR: Failed to bind value to column %d\n", col);
        ret = -1;
        goto done;
    }

    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not insert subtitle stream info: %s\n",
                sqlite3_errmsg(ldv->db));
        ret = -1;
        goto done;
    }

    ret = 0;

done:
    lms_db_reset_stmt(stmt);
    return ret;
}

static int
_db_insert_stream_audio(lms_db_video_t *ldv, int64_t video_id,
                        const struct lms_stream *s)
{
    sqlite3_stmt *stmt;
    int col = 0, ret;

    stmt = ldv->insert_audio_streams;

    if (lms_db_bind_int64(stmt, ++col, video_id) ||
        lms_db_bind_int(stmt, ++col, s->stream_id) ||
        lms_db_bind_text(stmt, ++col, s->codec.str, s->codec.len) ||
        lms_db_bind_text(stmt, ++col, s->lang.str, s->lang.len) ||
        lms_db_bind_int(stmt, ++col, s->audio.channels) ||
        lms_db_bind_int(stmt, ++col, s->audio.sampling_rate) ||
        lms_db_bind_int(stmt, ++col, s->audio.bitrate)) {
        fprintf(stderr, "ERROR: Failed to bind value to column %d\n", col);
        ret = -1;
        goto done;
    }

    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not insert audio stream info: %s\n",
                sqlite3_errmsg(ldv->db));
        ret = -1;
        goto done;
    }

    ret = 0;

done:
    lms_db_reset_stmt(stmt);
    return ret;
}

static int
_db_insert_stream_video(lms_db_video_t *ldv, int64_t video_id,
                        const struct lms_stream *s)
{
    sqlite3_stmt *stmt;
    int col = 0, ret;

    stmt = ldv->insert_video_streams;

    if (lms_db_bind_int64(stmt, ++col, video_id) ||
        lms_db_bind_int(stmt, ++col, s->stream_id) ||
        lms_db_bind_text(stmt, ++col, s->codec.str, s->codec.len) ||
        lms_db_bind_text(stmt, ++col, s->lang.str, s->lang.len) ||
        lms_db_bind_text(stmt, ++col, s->video.aspect_ratio.str,
                         s->video.aspect_ratio.len) ||
        lms_db_bind_int(stmt, ++col, s->video.bitrate) ||
        lms_db_bind_double(stmt, ++col, s->video.framerate) ||
        lms_db_bind_int(stmt, ++col, s->video.interlaced) ||
        lms_db_bind_int(stmt, ++col, s->video.width) ||
        lms_db_bind_int(stmt, ++col, s->video.height)) {
        fprintf(stderr, "ERROR: Failed to bind value to column %d\n", col);
        ret = -1;
        goto done;
    }

    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not insert video stream info: %s\n",
                sqlite3_errmsg(ldv->db));
        ret = -1;
        goto done;
    }

    ret = 0;

done:
    lms_db_reset_stmt(stmt);
    return ret;
}

static int
_db_insert(lms_db_video_t *ldv, const struct lms_video_info *info)
{
    sqlite3_stmt *stmt;
    int r, ret;

    stmt = ldv->insert;

    ret = lms_db_bind_int64(stmt, 1, info->id);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 2, info->title.str, info->title.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 3, info->artist.str, info->artist.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 4, info->length);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 5, info->container.str,
                           info->container.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 6, info->dlna_profile.str,
                           info->dlna_profile.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 7, info->dlna_mime.str, info->dlna_mime.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int64(stmt, 8, info->packet_size);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not insert video info: %s\n",
                sqlite3_errmsg(ldv->db));
        ret = -4;
        goto done;
    }

    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

/**
 * Add video file to DB.
 *
 * This is usually called from plugin's @b parse() callback.
 *
 * @param ldv handle returned by lms_db_video_new().
 * @param info video information to store.
 *
 * @return On success 0 is returned.
 * @ingroup LMS_Plugins
 */
int
lms_db_video_add(lms_db_video_t *ldv, struct lms_video_info *info, const char *path)
{
    struct lms_stream *s;
    const struct lms_dlna_video_profile *dlna;
    int r;

    if (!ldv)
        return -1;
    if (!info)
        return -2;
    if (info->id < 1)
        return -3;

    if (info->dlna_mime.len == 0 && info->dlna_profile.len == 0) {
        dlna = lms_dlna_get_video_profile(info);
        if (dlna) {
            info->dlna_mime = *dlna->dlna_mime;
            info->dlna_profile = *dlna->dlna_profile;
        } else {
            info->dlna_mime.str = (char *)magic_file(ldv->magic, path);
            if (info->dlna_mime.str) {
                info->dlna_mime.len = strlen(info->dlna_mime.str);
            }
        }
    }

    r = _db_insert(ldv, info);
    if (r < 0)
        return r;

    for (s = info->streams; s; s = s->next) {
        switch (s->type) {
        case LMS_STREAM_TYPE_AUDIO:
            r = _db_insert_stream_audio(ldv, info->id, s);
            break;
        case LMS_STREAM_TYPE_VIDEO:
            r = _db_insert_stream_video(ldv, info->id, s);
            break;
        case LMS_STREAM_TYPE_SUBTITLE:
            r = _db_insert_stream_subtitle(ldv, info->id, s);
            break;
        case LMS_STREAM_TYPE_UNKNOWN:
            fprintf(stderr, "WARNING: Ignoring unknown stream type\n");
            r = 0;
            break;
        }

        if (r < 0) {
            fprintf(stderr, "ERROR: Failed to insert stream #%d type %d\n",
                    s->stream_id, s->type);
            break;
        }
    }

    return r;
}

/**
 * If aspect ratio wasn't set yet, guess it from stream video info size.
 *
 * This function calls lms_aspect_ratio_guess() and thus the
 * aspect_ratio string will be allocated with malloc(). Remember to
 * free() it afterwards.
 *
 * @param info where to query width and height, then setting aspect_ratio string.
 *
 * @return 1 on success, 0 on failure.
 */
int
lms_stream_video_info_aspect_ratio_guess(struct lms_stream_video_info *info)
{
    if (info->aspect_ratio.len > 0)
        return 1;

    return lms_aspect_ratio_guess(&info->aspect_ratio,
                                  info->width,
                                  info->height);
}
