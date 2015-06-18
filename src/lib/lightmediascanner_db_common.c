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
#include <string.h>

#if SQLITE_VERSION_NUMBER < 3003009
int
sqlite3_prepare_v2(sqlite3 *db, const char *sql, int len, sqlite3_stmt **stmt, const char **tail)
{
    return sqlite3_prepare(db, sql, len, stmt, tail);
}
#endif /* SQLITE_VERSION_NUMBER < 3003009 */

#if SQLITE_VERSION_NUMBER < 3003007
int
sqlite3_clear_bindings(sqlite3_stmt *stmt)
{
    int i, last;
    int rc;

    rc = SQLITE_OK;
    last = sqlite3_bind_parameter_count(stmt);
    for(i = 1; rc == SQLITE_OK && i <= last; i++) {
        rc = sqlite3_bind_null(stmt, i);
    }
    return rc;
}
#endif /* SQLITE_VERSION_NUMBER < 3003007 */

#if SQLITE_VERSION_NUMBER < 3003008
/* Until 3.3.8 it doesn't support CREATE TRIGGER IF NOT EXISTS, so
 * just ignore errors :-(
 */
int
lms_db_create_trigger_if_not_exists(sqlite3 *db, const char *sql)
{
    char *errmsg, *query;
    int r, sql_len, prefix_len;

    prefix_len = sizeof("CREATE TRIGGER ") - 1;
    sql_len = strlen(sql);
    query = malloc((prefix_len + sql_len + 1) * sizeof(char));
    if (!query)
        return -1;

    memcpy(query, "CREATE TRIGGER ", prefix_len);
    memcpy(query + prefix_len, sql, sql_len + 1);
    r = sqlite3_exec(db, query, NULL, NULL, &errmsg);
    free(query);
    if (r != SQLITE_OK)
        sqlite3_free(errmsg);
    return 0;
}
#else /* SQLITE_VERSION_NUMBER < 3003008 */
int
lms_db_create_trigger_if_not_exists(sqlite3 *db, const char *sql)
{
    char *errmsg, *query;
    int r, sql_len, prefix_len;

    prefix_len = sizeof("CREATE TRIGGER IF NOT EXISTS ") - 1;
    sql_len = strlen(sql);
    query = malloc((prefix_len + sql_len + 1) * sizeof(char));
    if (!query)
        return -1;

    memcpy(query, "CREATE TRIGGER IF NOT EXISTS ", prefix_len);
    memcpy(query + prefix_len, sql, sql_len + 1);
    r = sqlite3_exec(db, query, NULL, NULL, &errmsg);
    free(query);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create trigger: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -2;
    }
    return 0;
}
#endif /* SQLITE_VERSION_NUMBER < 3003008 */

sqlite3_stmt *
lms_db_compile_stmt(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        fprintf(stderr, "ERROR: could not prepare \"%s\": %s\n", sql,
                sqlite3_errmsg(db));

    return stmt;
}

int
lms_db_finalize_stmt(sqlite3_stmt *stmt, const char *name)
{
    int r;

    r = sqlite3_finalize(stmt);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not finalize %s statement: #%d\n",
                name, r);
        return -1;
    }

    return 0;
}

int
lms_db_reset_stmt(sqlite3_stmt *stmt)
{
    int r, ret;

    ret = r = sqlite3_reset(stmt);
    if (r != SQLITE_OK)
        fprintf(stderr, "ERROR: could not reset SQL statement: #%d\n", r);

    r = sqlite3_clear_bindings(stmt);
    ret += r;
    if (r != SQLITE_OK)
        fprintf(stderr, "ERROR: could not clear SQL: #%d\n", r);

    return ret;
}

int
lms_db_bind_text(sqlite3_stmt *stmt, int col, const char *text, int len)
{
    int r;

    if (text)
        r = sqlite3_bind_text(stmt, col, text, len, SQLITE_STATIC);
    else
        r = sqlite3_bind_null(stmt, col);

    if (r == SQLITE_OK)
        return 0;
    else {
        sqlite3 *db;
        const char *err;

        db = sqlite3_db_handle(stmt);
        err = sqlite3_errmsg(db);
        fprintf(stderr, "ERROR: could not bind SQL value %d: %s\n", col, err);
        return -col;
    }
}

int
lms_db_bind_blob(sqlite3_stmt *stmt, int col, const void *blob, int len)
{
    int r;

    if (blob)
        r = sqlite3_bind_blob(stmt, col, blob, len, SQLITE_STATIC);
    else
        r = sqlite3_bind_null(stmt, col);

    if (r == SQLITE_OK)
        return 0;
    else {
        sqlite3 *db;
        const char *err;

        db = sqlite3_db_handle(stmt);
        err = sqlite3_errmsg(db);
        fprintf(stderr, "ERROR: could not bind SQL value %d: %s\n", col, err);
        return -col;
    }
}

int
lms_db_bind_int64(sqlite3_stmt *stmt, int col, int64_t value)
{
    int r;

    r = sqlite3_bind_int64(stmt, col, value);
    if (r == SQLITE_OK)
        return 0;
    else {
        sqlite3 *db;
        const char *err;

        db = sqlite3_db_handle(stmt);
        err = sqlite3_errmsg(db);
        fprintf(stderr, "ERROR: could not bind SQL value %d: %s\n", col, err);
        return -col;
    }
}

int
lms_db_bind_int64_or_null(sqlite3_stmt *stmt, int col, int64_t *p_value)
{
    int r;

    if (p_value)
        r = sqlite3_bind_int64(stmt, col, *p_value);
    else
        r = sqlite3_bind_null(stmt, col);
    if (r == SQLITE_OK)
        return 0;
    else {
        sqlite3 *db;
        const char *err;

        db = sqlite3_db_handle(stmt);
        err = sqlite3_errmsg(db);
        fprintf(stderr, "ERROR: could not bind SQL value %d: %s\n", col, err);
        return -col;
    }
}

int
lms_db_bind_int(sqlite3_stmt *stmt, int col, int value)
{
    int r;

    r = sqlite3_bind_int(stmt, col, value);
    if (r == SQLITE_OK)
        return 0;
    else {
        sqlite3 *db;
        const char *err;

        db = sqlite3_db_handle(stmt);
        err = sqlite3_errmsg(db);
        fprintf(stderr, "ERROR: could not bind SQL value %d: %s\n", col, err);
        return -col;
    }
}

int
lms_db_bind_double(sqlite3_stmt *stmt, int col, double value)
{
    int r;

    r = sqlite3_bind_double(stmt, col, value);
    if (r == SQLITE_OK)
        return 0;
    else {
        sqlite3 *db;
        const char *err;

        db = sqlite3_db_handle(stmt);
        err = sqlite3_errmsg(db);
        fprintf(stderr, "ERROR: could not bind SQL value %d: %s\n", col, err);
        return -col;
    }
}

int
lms_db_update_id_get(sqlite3 *db)
{
    return lms_db_table_version_get(db, "update_id");
}

int
lms_db_update_id_set(sqlite3 *db, unsigned int update_id)
{
    return lms_db_table_version_set(db, "update_id", update_id);
}

int
lms_db_table_version_get(sqlite3 *db, const char *table)
{
    int r, version;
    sqlite3_stmt *stmt;

    stmt = lms_db_compile_stmt(db,
         "SELECT version FROM lms_internal WHERE tab = ?");
    if (!stmt)
        return -1;

    if (lms_db_bind_text(stmt, 1, table, -1) != 0) {
        version = -1;
        goto done;
    }

    r = sqlite3_step(stmt);
    if (r == SQLITE_DONE)
        version = 0;
    else if (r == SQLITE_ROW)
        version = sqlite3_column_int(stmt, 0);
    else {
        version = -1;
        fprintf(stderr, "ERROR: could not get table '%s' version: %s\n",
                table, sqlite3_errmsg(db));
    }

  done:
    lms_db_reset_stmt(stmt);
    lms_db_finalize_stmt(stmt, "table_version_get");

    return version;
}

int
lms_db_table_version_set(sqlite3 *db, const char *table, unsigned int version)
{
    int r, ret;
    sqlite3_stmt *stmt;

    stmt = lms_db_compile_stmt(db,
        "INSERT OR REPLACE INTO lms_internal (tab, version) VALUES (?, ?)");
    if (!stmt)
        return -1;

    ret = lms_db_bind_text(stmt, 1, table, -1);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 2, version);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        ret = -1;
        fprintf(stderr, "ERROR: could not set table '%s' version: %s\n",
                table, sqlite3_errmsg(db));
    }

  done:
    lms_db_reset_stmt(stmt);
    lms_db_finalize_stmt(stmt, "table_version_set");

    return ret;
}

int
lms_db_table_update(sqlite3 *db, const char *table, unsigned int current_version, unsigned int last_version, const lms_db_table_updater_t *updaters)
{
    if (current_version == last_version)
        return 0;
    else if (current_version > last_version) {
        fprintf(stderr,
                "WARNING: current version (%d) of table '%s' is greater than "
                "last known version (%d), no updates will be made.\n",
                current_version, table, last_version);
        return 0;
    }

    for (; current_version < last_version; current_version++) {
        int r, is_last_run;

        is_last_run = current_version == (last_version - 1);
        r = updaters[current_version](db, table, current_version, is_last_run);
        if (r != 0) {
            fprintf(stderr,
                    "ERROR: could not update table '%s' from version %d->%d\n",
                    table, current_version, current_version + 1);
            return r;
        }
        lms_db_table_version_set(db, table, current_version + 1);
    }

    return 0;
}

int
lms_db_table_update_if_required(sqlite3 *db, const char *table, unsigned int last_version, lms_db_table_updater_t *updaters)
{
    int current_version;

    current_version = lms_db_table_version_get(db, table);
    if (current_version < 0)
        return -1;
    else
        return lms_db_table_update(db, table, current_version, last_version,
                                   updaters);
}

static int
lms_db_cache_find_db(const struct lms_db_cache *cache, const sqlite3 *db)
{
    int i;

    for (i = 0; i < cache->size; i++)
        if (cache->entries[i].db == db)
            return i;

    return -1;
}

static int
lms_db_cache_resize(struct lms_db_cache *cache, unsigned int new_size)
{
    void *tmp = realloc(cache->entries,
                        cache->size * sizeof(*cache->entries));
    if (new_size > 0 && !tmp) {
        perror("realloc");
        return -1;
    }
    cache->size = new_size;
    cache->entries = tmp;
    return 0;
}

int
lms_db_cache_add(struct lms_db_cache *cache, const sqlite3 *db, void *data)
{
    struct lms_db_cache_entry *e;
    int idx;

    idx = lms_db_cache_find_db(cache, db);
    if (idx >= 0) {
        e = cache->entries + idx;
        if (e->data == data)
            return 0;
        else {
            fprintf(stderr,
                    "ERROR: cache %p for db %p has another data registered"
                    ": %p (current is %p)\n", cache, db, e->data, data);
            return -1;
        }
    }

    idx = cache->size;
    if (lms_db_cache_resize(cache, cache->size + 1) != 0) {
        return -2;
    }

    e = cache->entries + idx;
    e->db = db;
    e->data = data;
    return 0;
}

int
lms_db_cache_del(struct lms_db_cache *cache, const sqlite3 *db, void *data)
{
    int idx;
    struct lms_db_cache_entry *e;

    idx = lms_db_cache_find_db(cache, db);
    if (idx < 0) {
        fprintf(stderr, "ERROR: no db %p found in cache %p\n", db, cache);
        return -1;
    }

    e = cache->entries + idx;
    if (e->data != data) {
        fprintf(stderr, "ERROR: data mismatch in request to delete from cache: "
                "want %p, has %p, cache %p, db %p\n", data, e->data, cache, db);
        return -2;
    }

    for (; idx < cache->size - 1; idx++)
        cache->entries[idx] = cache->entries[idx + 1];

    return lms_db_cache_resize(cache, cache->size - 1);
}

int
lms_db_cache_get(struct lms_db_cache *cache, const sqlite3 *db, void **pdata)
{
    int idx;

    idx = lms_db_cache_find_db(cache, db);
    if (idx < 0)
        return -1;

    *pdata = cache->entries[idx].data;
    return 0;
}

static int
_db_table_updater_files_0(sqlite3 *db, const char *table, unsigned int current_version, int is_last_run) {
    return 0;
}

static int
_db_table_updater_files_1(sqlite3 *db, const char *table, unsigned int current_version, int is_last_run) {
    char *errmsg;
    int r;

    errmsg = NULL;
    r = sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS files ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "path BLOB NOT NULL UNIQUE, "
                     "mtime INTEGER NOT NULL, "
                     "dtime INTEGER NOT NULL, "
                     "itime INTEGER NOT NULL, "
                     "size INTEGER NOT NULL"
                     ")",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create 'files' table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    r = sqlite3_exec(db,
                     "CREATE INDEX IF NOT EXISTS files_path_idx ON files ("
                     "path"
                     ")",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create 'files_path_idx' index: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -2;
    }

    return 0;
}

static int
_db_table_updater_files_2(sqlite3 *db, const char *table, unsigned int current_version, int is_last_run)
{
    char *errmsg = NULL;
    int r;

    r = sqlite3_exec(db,
                     "ALTER TABLE files ADD COLUMN update_id INTEGER DEFAULT 0",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: add update_id field into 'files' table: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    return 0;
}

static lms_db_table_updater_t _db_table_updater_files[] = {
    _db_table_updater_files_0,
    _db_table_updater_files_1,
    _db_table_updater_files_2,
};

int
lms_db_create_core_tables_if_required(sqlite3 *db)
{
    char *errmsg;
    int r;

    errmsg = NULL;
    r = sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS lms_internal ("
                     "tab TEXT NOT NULL UNIQUE, "
                     "version INTEGER NOT NULL"
                     ")",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create 'lms_internal' table: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    r = lms_db_table_update_if_required(db, "files",
                                        LMS_ARRAY_SIZE(_db_table_updater_files),
                                        _db_table_updater_files);
    return r;
}


sqlite3_stmt *
lms_db_compile_stmt_begin_transaction(sqlite3 *db)
{
    return lms_db_compile_stmt(db, "BEGIN TRANSACTION");
}

int
lms_db_begin_transaction(sqlite3_stmt *stmt)
{
    int r, ret;

    ret = 0;
    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not begin transaction: %s\n",
                sqlite3_errmsg(sqlite3_db_handle(stmt)));
        ret = -1;
    }

    r = sqlite3_reset(stmt);
    if (r != SQLITE_OK)
        fprintf(stderr, "ERROR: could not reset SQL statement: %s\n",
                sqlite3_errmsg(sqlite3_db_handle(stmt)));

    return ret;
}

sqlite3_stmt *
lms_db_compile_stmt_end_transaction(sqlite3 *db)
{
    return lms_db_compile_stmt(db, "COMMIT");
}

int
lms_db_end_transaction(sqlite3_stmt *stmt)
{
    int r, ret;

    ret = 0;
    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not end transaction: %s\n",
                sqlite3_errmsg(sqlite3_db_handle(stmt)));
        ret = -1;
    }

    r = sqlite3_reset(stmt);
    if (r != SQLITE_OK)
        fprintf(stderr, "ERROR: could not reset SQL statement: %s\n",
                sqlite3_errmsg(sqlite3_db_handle(stmt)));

    return ret;
}

sqlite3_stmt *
lms_db_compile_stmt_get_file_info(sqlite3 *db)
{
    return lms_db_compile_stmt(db,
        "SELECT id, mtime, dtime, itime, size FROM files WHERE path = ?");
}

int
lms_db_get_file_info(sqlite3_stmt *stmt, struct lms_file_info *finfo)
{
    int r, ret;

    ret = lms_db_bind_blob(stmt, 1, finfo->path, finfo->path_len);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r == SQLITE_DONE) {
        ret = 1;
        finfo->id = -1;
        goto done;
    }

    if (r != SQLITE_ROW) {
        fprintf(stderr, "ERROR: could not get file info from table: %s\n",
                sqlite3_errmsg(sqlite3_db_handle(stmt)));
        ret = -2;
        goto done;
    }

    finfo->id = sqlite3_column_int64(stmt, 0);
    finfo->mtime = sqlite3_column_int(stmt, 1);
    finfo->dtime = sqlite3_column_int(stmt, 2);
    finfo->itime = sqlite3_column_int(stmt, 3);
    finfo->size = sqlite3_column_int(stmt, 4);
    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

sqlite3_stmt *
lms_db_compile_stmt_update_file_info(sqlite3 *db)
{
    return lms_db_compile_stmt(db,
        "UPDATE files SET mtime = ?, dtime = ?, itime = ?, size = ?, update_id = ? WHERE id = ?");
}

int
lms_db_update_file_info(sqlite3_stmt *stmt, const struct lms_file_info *finfo, unsigned int update_id)
{
    int r, ret;

    ret = lms_db_bind_int(stmt, 1, finfo->mtime);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 2, finfo->dtime);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 3, finfo->itime);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 4, finfo->size);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 5, update_id);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 6, finfo->id);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not update file info: %s\n",
                sqlite3_errmsg(sqlite3_db_handle(stmt)));
        ret = -5;
        goto done;
    }

    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

sqlite3_stmt *
lms_db_compile_stmt_insert_file_info(sqlite3 *db)
{
    return lms_db_compile_stmt(db,
        "INSERT INTO files (path, mtime, dtime, itime, size, update_id) VALUES(?, ?, ?, ?, ?, ?)");
}

int
lms_db_insert_file_info(sqlite3_stmt *stmt, struct lms_file_info *finfo,
                        unsigned int update_id)
{
    int r, ret;

    ret = lms_db_bind_blob(stmt, 1, finfo->path, finfo->path_len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 2, finfo->mtime);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 3, finfo->dtime);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 4, finfo->itime);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 5, finfo->size);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 6, update_id);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not insert file info: %s\n",
                sqlite3_errmsg(sqlite3_db_handle(stmt)));
        ret = -5;
        goto done;
    }

    finfo->id = sqlite3_last_insert_rowid(sqlite3_db_handle(stmt));
    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

sqlite3_stmt *
lms_db_compile_stmt_delete_file_info(sqlite3 *db)
{
    return lms_db_compile_stmt(db, "DELETE FROM files WHERE id = ?");
}

int
lms_db_delete_file_info(sqlite3_stmt *stmt, const struct lms_file_info *finfo)
{
    int r, ret;

    ret = lms_db_bind_int64(stmt, 1, finfo->id);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not delete file info: %s\n",
                sqlite3_errmsg(sqlite3_db_handle(stmt)));
        ret = -2;
        goto done;
    }
    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

sqlite3_stmt *
lms_db_compile_stmt_set_file_dtime(sqlite3 *db)
{
    return lms_db_compile_stmt(db, "UPDATE files SET dtime = ?, itime = ? WHERE id = ?");
}

int
lms_db_set_file_dtime(sqlite3_stmt *stmt, const struct lms_file_info *finfo)
{
    int r, ret;

    ret = lms_db_bind_int(stmt, 1, finfo->dtime);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 2, finfo->itime);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int64(stmt, 3, finfo->id);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not set file dtime: %s\n",
                sqlite3_errmsg(sqlite3_db_handle(stmt)));
        ret = -3;
        goto done;
    }

    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

sqlite3_stmt *
lms_db_compile_stmt_get_files(sqlite3 *db)
{
    return lms_db_compile_stmt(db,
        "SELECT id, path, mtime, dtime, itime, size FROM files WHERE path LIKE ?");
}

int
lms_db_get_files(sqlite3_stmt *stmt, const char *path, int len)
{
    int ret;

    ret = lms_db_bind_blob(stmt, 1, path, len);
    return ret;
}
