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
