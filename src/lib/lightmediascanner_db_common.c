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
        version = sqlite3_column_int(stmt, 1);
    else {
        version = -1;
        fprintf(stderr, "ERROR: could not get table '%s' version: %s",
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
        fprintf(stderr, "ERROR: could not set table '%s' version: %s",
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
