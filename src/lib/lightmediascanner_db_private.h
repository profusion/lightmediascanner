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

#ifndef _LIGHTMEDIASCANNER_DB_PRIVATE_H_
#define _LIGHTMEDIASCANNER_DB_PRIVATE_H_ 1

#ifdef API
#undef API
#endif

#ifdef __GNUC__
# if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#  define GNUC_NON_NULL(...) __attribute__((nonnull(__VA_ARGS__)))
# else
#  define GNUC_NON_NULL(...)
# endif
#else
#  define GNUC_NON_NULL(...)
#endif

#include <sqlite3.h>
#include <sys/types.h>

sqlite3_stmt *lms_db_compile_stmt(sqlite3 *db, const char *sql) GNUC_NON_NULL(1, 2);
int lms_db_finalize_stmt(sqlite3_stmt *stmt, const char *name) GNUC_NON_NULL(1, 2);
int lms_db_reset_stmt(sqlite3_stmt *stmt) GNUC_NON_NULL(1);
int lms_db_bind_text(sqlite3_stmt *stmt, int col, const char *text, int len) GNUC_NON_NULL(1);
int lms_db_bind_blob(sqlite3_stmt *stmt, int col, const void *blob, int len) GNUC_NON_NULL(1);
int lms_db_bind_int64(sqlite3_stmt *stmt, int col, int64_t value) GNUC_NON_NULL(1);
int lms_db_bind_int64_or_null(sqlite3_stmt *stmt, int col, int64_t *p_value) GNUC_NON_NULL(1);
int lms_db_bind_int(sqlite3_stmt *stmt, int col, int value) GNUC_NON_NULL(1);
int lms_db_bind_double(sqlite3_stmt *stmt, int col, double value) GNUC_NON_NULL(1);
int lms_db_create_trigger_if_not_exists(sqlite3 *db, const char *sql) GNUC_NON_NULL(1, 2);

#endif /* _LIGHTMEDIASCANNER_DB_PRIVATE_H_ */
