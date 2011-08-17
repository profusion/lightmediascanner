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
#include <stdlib.h>
#include <stdio.h>

struct lms_db_playlist {
    sqlite3 *db;
    sqlite3_stmt *insert;
    unsigned int _references;
    unsigned int _is_started:1;
};

static struct lms_db_cache _cache = {0, NULL};

static int
_db_table_updater_playlists_0(sqlite3 *db, const char *table, unsigned int current_version, int is_last_run) {
    char *errmsg;
    int r, ret;

    errmsg = NULL;
    r = sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS playlists ("
                     "id INTEGER PRIMARY KEY, "
                     "title TEXT, "
                     "n_entries INTEGER NOT NULL"
                     ")",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create 'playlists' table: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    r = sqlite3_exec(db,
                     "CREATE INDEX IF NOT EXISTS playlists_title_idx ON "
                     "playlists (title)",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr,
                "ERROR: could not create 'playlists_title_idx' index: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -2;
    }

    ret = lms_db_create_trigger_if_not_exists(db,
        "delete_playlists_on_files_deleted "
        "DELETE ON files FOR EACH ROW BEGIN "
        " DELETE FROM playlists WHERE id = OLD.id; END;");
    if (ret != 0)
        goto done;

    ret = lms_db_create_trigger_if_not_exists(db,
        "delete_files_on_playlists_deleted "
        "DELETE ON playlists FOR EACH ROW BEGIN "
        " DELETE FROM files WHERE id = OLD.id; END;");

  done:
    return ret;
}

static lms_db_table_updater_t _db_table_updater_playlists[] = {
    _db_table_updater_playlists_0
};


static int
_db_create_table_if_required(sqlite3 *db)
{
    return lms_db_table_update_if_required(db, "playlists",
         LMS_ARRAY_SIZE(_db_table_updater_playlists),
         _db_table_updater_playlists);
}

/**
 * Create playlist DB access tool.
 *
 * Creates or get a reference to tools to access 'playlists' table in an
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
lms_db_playlist_t *
lms_db_playlist_new(sqlite3 *db)
{
    lms_db_playlist_t *ldp;
    void *p;

    if (lms_db_cache_get(&_cache, db, &p) == 0) {
        ldp = p;
        ldp->_references++;
        return ldp;
    }

    if (!db)
        return NULL;

    if (_db_create_table_if_required(db) != 0) {
        fprintf(stderr, "ERROR: could not create table.\n");
        return NULL;
    }

    ldp = calloc(1, sizeof(lms_db_playlist_t));
    ldp->_references = 1;
    ldp->db = db;

    if (lms_db_cache_add(&_cache, db, ldp) != 0) {
        lms_db_playlist_free(ldp);
        return NULL;
    }

    return ldp;
}

/**
 * Start playlist DB access tool.
 *
 * Compile SQL statements and other initialization functions.
 *
 * This is usually called from plugin's @b start() callback.
 *
 * @param ldp handle returned by lms_db_playlist_new().
 *
 * @return On success 0 is returned.
 * @ingroup LMS_Plugins
 */
int
lms_db_playlist_start(lms_db_playlist_t *ldp)
{
    if (!ldp)
        return -1;
    if (ldp->_is_started)
        return 0;

    ldp->insert = lms_db_compile_stmt(ldp->db,
        "INSERT OR REPLACE INTO playlists (id, title, n_entries) "
        "VALUES (?, ?, ?)");
    if (!ldp->insert)
        return -2;

    ldp->_is_started = 1;
    return 0;
}

/**
 * Free playlist DB access tool.
 *
 * Unreference and possible free resources allocated to access tool.
 *
 * This is usually called from plugin's @b finish() callback.
 *
 * @param ldp handle returned by lms_db_playlist_new().
 *
 * @return On success 0 is returned.
 * @ingroup LMS_Plugins
 */
int
lms_db_playlist_free(lms_db_playlist_t *ldp)
{
    int r;

    if (!ldp)
        return -1;
    if (ldp->_references == 0) {
        fprintf(stderr, "ERROR: over-called lms_db_playlist_free(%p)\n", ldp);
        return -1;
    }

    ldp->_references--;
    if (ldp->_references > 0)
        return 0;

    if (ldp->insert)
        lms_db_finalize_stmt(ldp->insert, "insert");

    r = lms_db_cache_del(&_cache, ldp->db, ldp);
    free(ldp);

    return r;
}

static int
_db_insert(lms_db_playlist_t *ldp, const struct lms_playlist_info *info)
{
    sqlite3_stmt *stmt;
    int r, ret;

    stmt = ldp->insert;

    ret = lms_db_bind_int64(stmt, 1, info->id);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 2, info->title.str, info->title.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 3, info->n_entries);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not insert playlist info: %s\n",
                sqlite3_errmsg(ldp->db));
        ret = -4;
        goto done;
    }

    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

/**
 * Add playlist file to DB.
 *
 * This is usually called from plugin's @b parse() callback.
 *
 * @param ldp handle returned by lms_db_playlist_new().
 * @param info playlist information to store.
 *
 * @return On success 0 is returned.
 * @ingroup LMS_Plugins
 */
int
lms_db_playlist_add(lms_db_playlist_t *ldp, struct lms_playlist_info *info)
{
    if (!ldp)
        return -1;
    if (!info)
        return -2;
    if (info->id < 1)
        return -3;

    return _db_insert(ldp, info);
}
