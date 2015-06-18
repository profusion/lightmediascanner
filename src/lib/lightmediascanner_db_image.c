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
#include "lightmediascanner_db_private.h"
#include <lightmediascanner_dlna.h>
#include <stdlib.h>
#include <stdio.h>

struct lms_db_image {
    sqlite3 *db;
    sqlite3_stmt *insert;
    unsigned int _references;
    unsigned int _is_started:1;
};

static struct lms_db_cache _cache = { };

static int
_db_table_updater_images_0(sqlite3 *db, const char *table, unsigned int current_version, int is_last_run) {
    char *errmsg;
    int r, ret;

    errmsg = NULL;
    r = sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS images ("
                     "id INTEGER PRIMARY KEY, "
                     "title TEXT, "
                     "artist TEXT, "
                     "date INTEGER NOT NULL, "
                     "width INTEGER NOT NULL, "
                     "height INTEGER NOT NULL, "
                     "orientation INTEGER NOT NULL, "
                     "gps_lat REAL DEFAULT 0.0, "
                     "gps_long REAL DEFAULT 0.0, "
                     "gps_alt REAL DEFAULT 0.0"
                     ")",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create 'images' table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    r = sqlite3_exec(db,
                     "CREATE INDEX IF NOT EXISTS images_date_idx ON images ("
                     "date"
                     ")",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create 'images_date_idx' index: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -2;
    }

    ret = lms_db_create_trigger_if_not_exists(db,
        "delete_images_on_files_deleted "
        "DELETE ON files FOR EACH ROW BEGIN "
        " DELETE FROM images WHERE id = OLD.id; END;");
    if (ret != 0)
        goto done;

    ret = lms_db_create_trigger_if_not_exists(db,
        "delete_files_on_images_deleted "
        "DELETE ON images FOR EACH ROW BEGIN "
        " DELETE FROM files WHERE id = OLD.id; END;");

  done:
    return ret;
}

static int
_db_table_updater_images_1(sqlite3 *db, const char *table, unsigned int current_version, int is_last_run)
{
    int ret;
    char *err;

    ret = sqlite3_exec(
        db, "BEGIN TRANSACTION;"
        "ALTER TABLE images ADD COLUMN dlna_profile TEXT DEFAULT NULL;"
        "ALTER TABLE images ADD COLUMN dlna_mime TEXT DEFAULT NULL;"
        "COMMIT;",
        NULL, NULL, &err);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "ERROR: could add columns to images table: %s\n", err);
        sqlite3_free(err);
    }

    return ret;
}

static int
_db_table_updater_images_2(sqlite3 *db, const char *table, unsigned int current_version, int is_last_run)
{
    int ret;
    char *err;

    ret = sqlite3_exec(
        db, "BEGIN TRANSACTION;"
        "ALTER TABLE images ADD COLUMN container TEXT DEFAULT NULL;"
        "COMMIT;",
        NULL, NULL, &err);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "ERROR: could add columns to images table: %s\n", err);
        sqlite3_free(err);
    }

    return ret;
}

static lms_db_table_updater_t _db_table_updater_images[] = {
    _db_table_updater_images_0,
    _db_table_updater_images_1,
    _db_table_updater_images_2
};


static int
_db_create_table_if_required(sqlite3 *db)
{
    return lms_db_table_update_if_required(db, "images",
         LMS_ARRAY_SIZE(_db_table_updater_images),
         _db_table_updater_images);
}

/**
 * Create image DB access tool.
 *
 * Creates or get a reference to tools to access 'images' table in an
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
lms_db_image_t *
lms_db_image_new(sqlite3 *db)
{
    lms_db_image_t *ldi;
    void *p;

    if (!db)
        return NULL;

    if (lms_db_cache_get(&_cache, db, &p) == 0) {
        ldi = p;
        ldi->_references++;
        return ldi;
    }

    if (_db_create_table_if_required(db) != 0) {
        fprintf(stderr, "ERROR: could not create table.\n");
        return NULL;
    }

    ldi = calloc(1, sizeof(lms_db_image_t));
    ldi->_references = 1;
    ldi->db = db;

    if (lms_db_cache_add(&_cache, db, ldi) != 0) {
        lms_db_image_free(ldi);
        return NULL;
    }

    return ldi;
}

/**
 * Start image DB access tool.
 *
 * Compile SQL statements and other initialization functions.
 *
 * This is usually called from plugin's @b start() callback.
 *
 * @param ldi handle returned by lms_db_image_new().
 *
 * @return On success 0 is returned.
 * @ingroup LMS_Plugins
 */
int
lms_db_image_start(lms_db_image_t *ldi)
{
    if (!ldi)
        return -1;
    if (ldi->_is_started)
        return 0;

    ldi->insert = lms_db_compile_stmt(ldi->db,
        "INSERT OR REPLACE INTO images ("
        "id, title, artist, date, width, height, orientation, "
        "gps_lat, gps_long, gps_alt, dlna_profile, dlna_mime, container) "
         "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    if (!ldi->insert)
        return -2;

    ldi->_is_started = 1;
    return 0;
}

/**
 * Free image DB access tool.
 *
 * Unreference and possible free resources allocated to access tool.
 *
 * This is usually called from plugin's @b finish() callback.
 *
 * @param ldi handle returned by lms_db_image_new().
 *
 * @return On success 0 is returned.
 * @ingroup LMS_Plugins
 */
int
lms_db_image_free(lms_db_image_t *ldi)
{
    int r;

    if (!ldi)
        return -1;
    if (ldi->_references == 0) {
        fprintf(stderr, "ERROR: over-called lms_db_image_free(%p)\n", ldi);
        return -1;
    }

    ldi->_references--;
    if (ldi->_references > 0)
        return 0;

    if (ldi->insert)
        lms_db_finalize_stmt(ldi->insert, "insert");

    r = lms_db_cache_del(&_cache, ldi->db, ldi);
    free(ldi);

    return r;
}

static int
_db_insert(lms_db_image_t *ldi, const struct lms_image_info *info)
{
    sqlite3_stmt *stmt;
    int r, ret;

    stmt = ldi->insert;

    ret = lms_db_bind_int64(stmt, 1, info->id);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 2, info->title.str, info->title.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 3, info->artist.str, info->artist.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 4, info->date);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 5, info->width);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 6, info->height);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 7, info->orientation);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_double(stmt, 8, info->gps.latitude);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_double(stmt, 9, info->gps.longitude);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_double(stmt, 10, info->gps.altitude);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 11, info->dlna_profile.str,
                           info->dlna_profile.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 12, info->dlna_mime.str, info->dlna_mime.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 13, info->container.str, info->container.len);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not insert image info: %s\n",
                sqlite3_errmsg(ldi->db));
        ret = -11;
        goto done;
    }

    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

/**
 * Add image file to DB.
 *
 * This is usually called from plugin's @b parse() callback.
 *
 * @param ldi handle returned by lms_db_image_new().
 * @param info image information to store.
 *
 * @return On success 0 is returned.
 * @ingroup LMS_Plugins
 */
int
lms_db_image_add(lms_db_image_t *ldi, struct lms_image_info *info)
{
    const struct lms_dlna_image_profile *dlna;

    if (!ldi)
        return -1;
    if (!info)
        return -2;
    if (info->id < 1)
        return -3;

    if (info->dlna_mime.len == 0 && info->dlna_profile.len == 0) {
        dlna = lms_dlna_get_image_profile(info);
        if (dlna) {
            info->dlna_mime = *dlna->dlna_mime;
            info->dlna_profile = *dlna->dlna_profile;
        }
    }

    return _db_insert(ldi, info);
}
