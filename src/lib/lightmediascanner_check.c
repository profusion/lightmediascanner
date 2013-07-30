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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lightmediascanner.h"
#include "lightmediascanner_private.h"
#include "lightmediascanner_db_private.h"

struct master_db {
    sqlite3 *handle;
    sqlite3_stmt *get_files;
};

struct slave_db {
    sqlite3 *handle;
    sqlite3_stmt *transaction_begin;
    sqlite3_stmt *transaction_commit;
    sqlite3_stmt *delete_file_info;
    sqlite3_stmt *update_file_info;
};

struct single_process_db {
    sqlite3 *handle;
    sqlite3_stmt *get_files;
    sqlite3_stmt *transaction_begin;
    sqlite3_stmt *transaction_commit;
    sqlite3_stmt *delete_file_info;
    sqlite3_stmt *update_file_info;
};

/***********************************************************************
 * Master-Slave communication.
 ***********************************************************************/

struct comm_finfo {
    int path_len;
    int base;
    int64_t id;
    time_t mtime;
    time_t dtime;
    size_t size;
    unsigned int flags;
#define COMM_FINFO_FLAG_OUTDATED 1
};

static int
_master_send_file(const struct fds *master, const struct lms_file_info finfo, unsigned int flags)
{
    struct comm_finfo ci;

    ci.path_len = finfo.path_len;
    ci.base = finfo.base;
    ci.id = finfo.id;
    ci.mtime = finfo.mtime;
    ci.dtime = finfo.dtime;
    ci.size = finfo.size;
    ci.flags = flags;

    if (write(master->w, &ci, sizeof(ci)) < 0) {
        perror("write");
        return -1;
    }

    if (write(master->w, finfo.path, finfo.path_len) < 0) {
        perror("write");
        return -1;
    }

    return 0;
}

static int
_master_send_finish(const struct fds *master)
{
    struct comm_finfo ci = {-1, -1, -1, -1, -1, -1, 0};

    if (write(master->w, &ci, sizeof(ci)) < 0) {
        perror("write");
        return -1;
    }

    return 0;
}

static int
_master_recv_reply(const struct fds *master, struct pollfd *pfd, int *reply, int timeout)
{
    int r;

    r = poll(pfd, 1, timeout);
    if (r < 0) {
        perror("poll");
        return -1;
    }

    if (r == 0)
        return 1;

    if (read(master->r, reply, sizeof(*reply)) != sizeof(*reply)) {
        perror("read");
        return -2;
    }

    return 0;
}

static int
_slave_send_reply(const struct fds *slave, int reply)
{
    if (write(slave->w, &reply, sizeof(reply)) == 0) {
        perror("write");
        return -1;
    }
    return 0;
}

static int
_slave_recv_file(const struct fds *slave, struct lms_file_info *finfo, unsigned int *flags)
{
    struct comm_finfo ci;
    static char path[PATH_SIZE + 1];
    int r;

    r = read(slave->r, &ci, sizeof(ci));
    if (r != sizeof(ci)) {
        perror("read");
        return -1;
    }

    finfo->path_len = ci.path_len;
    finfo->base = ci.base;
    finfo->id = ci.id;
    finfo->mtime = ci.mtime;
    finfo->dtime = ci.dtime;
    finfo->size = ci.size;
    finfo->path = NULL;
    *flags = ci.flags;

    if (ci.path_len == -1)
        return 0;

    if (ci.path_len > PATH_SIZE) {
        fprintf(stderr, "ERROR: path too long (%d/%d)\n",
                ci.path_len, PATH_SIZE);
        return -2;
    }

    r = read(slave->r, path, ci.path_len);
    if (r != ci.path_len) {
        fprintf(stderr, "ERROR: could not read whole path %d/%d\n",
                r, ci.path_len);
        return -3;
    }

    path[ci.path_len] = 0;
    finfo->path = path;
    return 0;
}


/***********************************************************************
 * Slave-side.
 ***********************************************************************/

static int
_slave_db_compile_all_stmts(struct slave_db *db)
{
    sqlite3 *handle;

    handle = db->handle;

    db->transaction_begin = lms_db_compile_stmt_begin_transaction(handle);
    if (!db->transaction_begin)
        return -1;

    db->transaction_commit = lms_db_compile_stmt_end_transaction(handle);
    if (!db->transaction_commit)
        return -2;

    db->delete_file_info = lms_db_compile_stmt_delete_file_info(handle);
    if (!db->delete_file_info)
        return -3;

    db->update_file_info = lms_db_compile_stmt_update_file_info(handle);
    if (!db->update_file_info)
        return -4;

    return 0;
}

static struct slave_db *
_slave_db_open(const char *db_path)
{
    struct slave_db *db;

    db = calloc(1, sizeof(*db));
    if (!db) {
        perror("calloc");
        return NULL;
    }

    if (sqlite3_open(db_path, &db->handle) != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not open DB \"%s\": %s\n",
                db_path, sqlite3_errmsg(db->handle));
        goto error;
    }

    return db;

  error:
    sqlite3_close(db->handle);
    free(db);
    return NULL;
}

static int
_slave_db_close(struct slave_db *db)
{
    if (db->transaction_begin)
        lms_db_finalize_stmt(db->transaction_begin, "transaction_begin");

    if (db->transaction_commit)
        lms_db_finalize_stmt(db->transaction_commit, "transaction_commit");

    if (db->delete_file_info)
        lms_db_finalize_stmt(db->delete_file_info, "delete_file_info");

    if (db->update_file_info)
        lms_db_finalize_stmt(db->update_file_info, "update_file_info");

    if (sqlite3_close(db->handle) != SQLITE_OK) {
        fprintf(stderr, "ERROR: clould not close DB (slave): %s\n",
                sqlite3_errmsg(db->handle));
        return -1;
    }
    free(db);

    return 0;
}

static int
_single_process_db_compile_all_stmts(struct single_process_db *db)
{
    sqlite3 *handle;

    handle = db->handle;

    db->get_files = lms_db_compile_stmt_get_files(handle);
    if (!db->get_files)
        return -1;

    db->transaction_begin = lms_db_compile_stmt_begin_transaction(handle);
    if (!db->transaction_begin)
        return -2;

    db->transaction_commit = lms_db_compile_stmt_end_transaction(handle);
    if (!db->transaction_commit)
        return -3;

    db->delete_file_info = lms_db_compile_stmt_delete_file_info(handle);
    if (!db->delete_file_info)
        return -4;

    db->update_file_info = lms_db_compile_stmt_update_file_info(handle);
    if (!db->update_file_info)
        return -5;

    return 0;
}

static struct single_process_db *
_single_process_db_open(const char *db_path)
{
    struct single_process_db *db;

    db = calloc(1, sizeof(*db));
    if (!db) {
        perror("calloc");
        return NULL;
    }

    if (sqlite3_open(db_path, &db->handle) != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not open DB \"%s\": %s\n",
                db_path, sqlite3_errmsg(db->handle));
        goto error;
    }

    if (lms_db_create_core_tables_if_required(db->handle) != 0) {
        fprintf(stderr, "ERROR: could not setup tables and indexes.\n");
        goto error;
    }

    if (_single_process_db_compile_all_stmts(db) != 0) {
        fprintf(stderr, "ERROR: could not compile statements.\n");
        goto error;
    }

    return db;

  error:
    sqlite3_close(db->handle);
    free(db);
    return NULL;
}

static int
_single_process_db_close(struct single_process_db *db)
{
    if (db->get_files)
        lms_db_finalize_stmt(db->get_files, "get_files");

    if (db->transaction_begin)
        lms_db_finalize_stmt(db->transaction_begin, "transaction_begin");

    if (db->transaction_commit)
        lms_db_finalize_stmt(db->transaction_commit, "transaction_commit");

    if (db->delete_file_info)
        lms_db_finalize_stmt(db->delete_file_info, "delete_file_info");

    if (db->update_file_info)
        lms_db_finalize_stmt(db->update_file_info, "update_file_info");

    if (sqlite3_close(db->handle) != SQLITE_OK) {
        fprintf(stderr, "ERROR: clould not close DB (slave): %s\n",
                sqlite3_errmsg(db->handle));
        return -1;
    }
    free(db);

    return 0;
}

static int
_init_sync_send(struct fds *fds)
{
    return _slave_send_reply(fds, 0);
}

static int
_slave_work_int(lms_t *lms, struct fds *fds, struct slave_db *db)
{
    struct lms_file_info finfo;
    void **parser_match;
    unsigned int counter, flags;
    int r;

    parser_match = malloc(lms->n_parsers * sizeof(*parser_match));
    if (!parser_match) {
        perror("malloc");
        return -6;
    }

    _init_sync_send(fds);

    counter = 0;
    lms_db_begin_transaction(db->transaction_begin);

    while (((r = _slave_recv_file(fds, &finfo, &flags)) == 0) &&
           finfo.path_len > 0) {
        r = lms_db_update_file_info(db->update_file_info, &finfo, 0);
        if (r < 0)
            fprintf(stderr, "ERROR: could not update path in DB\n");
        else if (flags & COMM_FINFO_FLAG_OUTDATED) {
            int used;

            used = lms_parsers_check_using(lms, parser_match, &finfo);
            if (!used)
                r = 0;
            else {
                r = lms_parsers_run(lms, db->handle, parser_match, &finfo);
                if (r < 0) {
                    fprintf(stderr, "ERROR: pid=%d failed to parse \"%s\".\n",
                            getpid(), finfo.path);
                    lms_db_delete_file_info(db->delete_file_info, &finfo);
                }
            }
        }

        _slave_send_reply(fds, r);
        counter++;
        if (counter > lms->commit_interval) {
            lms_db_end_transaction(db->transaction_commit);
            lms_db_begin_transaction(db->transaction_begin);
            counter = 0;
        }
    }

    free(parser_match);
    lms_db_end_transaction(db->transaction_commit);

    return r;
}

static int
_slave_work(struct pinfo *pinfo)
{
    lms_t *lms = pinfo->common.lms;
    struct fds *fds = &pinfo->slave;
    struct slave_db *db;
    int r;

    db = _slave_db_open(lms->db_path);
    if (!db)
        return -1;

    if (lms_parsers_setup(lms, db->handle) != 0) {
        fprintf(stderr, "ERROR: could not setup parsers.\n");
        r = -2;
        goto end;
    }

    if (_slave_db_compile_all_stmts(db) != 0) {
        fprintf(stderr, "ERROR: could not compile statements.\n");
        r = -3;
        goto end;
    }

    if (lms_parsers_start(lms, db->handle) != 0) {
        fprintf(stderr, "ERROR: could not start parsers.\n");
        r = -4;
        goto end;
    }
    if (lms->n_parsers < 1) {
        fprintf(stderr, "ERROR: no parser could be started, exit.\n");
        r = -5;
        goto end;
    }

    r = _slave_work_int(lms, fds, db);

  end:
    lms_parsers_finish(lms, db->handle);
    _slave_db_close(db);
    _init_sync_send(fds);

    return r;
}


/***********************************************************************
 * Master-side.
 ***********************************************************************/

static int
_master_db_compile_all_stmts(struct master_db *db)
{
    sqlite3 *handle;

    handle = db->handle;

    db->get_files = lms_db_compile_stmt_get_files(handle);
    if (!db->get_files)
        return -1;

    return 0;
}

static struct master_db *
_master_db_open(const char *db_path)
{
    struct master_db *db;

    db = calloc(1, sizeof(*db));
    if (!db) {
        perror("calloc");
        return NULL;
    }

    if (sqlite3_open(db_path, &db->handle) != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not open DB \"%s\": %s\n",
                db_path, sqlite3_errmsg(db->handle));
        goto error;
    }

    if (lms_db_create_core_tables_if_required(db->handle) != 0) {
        fprintf(stderr, "ERROR: could not setup tables and indexes.\n");
        goto error;
    }

    if (_master_db_compile_all_stmts(db) != 0) {
        fprintf(stderr, "ERROR: could not compile statements.\n");
        goto error;
    }

    return db;

  error:
    sqlite3_close(db->handle);
    free(db);
    return NULL;
}

static int
_master_db_close(struct master_db *db)
{
    if (db->get_files)
        lms_db_finalize_stmt(db->get_files, "get_files");

    if (sqlite3_close(db->handle) != SQLITE_OK) {
        fprintf(stderr, "ERROR: clould not close DB (master): %s\n",
                sqlite3_errmsg(db->handle));
        return -1;
    }
    free(db);

    return 0;
}

static void
_calc_base(struct lms_file_info *finfo)
{
    int i;

    for (i = finfo->path_len - 1; i >= 0; i--)
        if (finfo->path[i] == '/') {
            finfo->base = i;
            return;
        }
}

static inline void
_update_finfo_from_stmt(struct lms_file_info *finfo, sqlite3_stmt *stmt)
{
    finfo->id = sqlite3_column_int64(stmt, 0);
    finfo->path = sqlite3_column_blob(stmt, 1);
    finfo->path_len = sqlite3_column_bytes(stmt, 1);
    finfo->base = 0;
    finfo->mtime = sqlite3_column_int(stmt, 2);
    finfo->dtime = sqlite3_column_int(stmt, 3);
    finfo->size = sqlite3_column_int(stmt, 5);
}

static inline void
_update_finfo_from_stat(struct lms_file_info *finfo, const struct stat *st)
{
    finfo->mtime = st->st_mtime;
    finfo->size = st->st_size;
    finfo->dtime = 0;
    finfo->itime = time(NULL);
}

static inline void
_report_progress(struct cinfo *info, const struct lms_file_info *finfo, lms_progress_status_t status)
{
    lms_progress_callback_t cb;
    lms_t *lms = info->lms;

    cb = lms->progress.cb;
    if (!cb)
        return;

    cb(lms, finfo->path, finfo->path_len, status, lms->progress.data);
}

static int
_finfo_update(void *db_ptr, struct cinfo *info, struct lms_file_info *finfo, unsigned int *flags)
{
    struct master_db *db = db_ptr;
    struct stat st;

    _update_finfo_from_stmt(finfo, db->get_files);

    *flags = 0;
    if (stat(finfo->path, &st) == 0) {
        if (st.st_mtime == finfo->mtime && st.st_size == finfo->size) {
            if (finfo->dtime == 0) {
                _report_progress(info, finfo, LMS_PROGRESS_STATUS_UP_TO_DATE);
                return 0;
            } else {
                finfo->dtime = 0;
                finfo->itime = time(NULL);
            }
        } else {
            _update_finfo_from_stat(finfo, &st);
            *flags |= COMM_FINFO_FLAG_OUTDATED;
        }
    } else {
        if (finfo->dtime)
            return 0;
        else
            finfo->dtime = time(NULL);
    }

    _calc_base(finfo);

    return 1;
}

static int
_check_row(void *db_ptr, struct cinfo *info)
{
    struct pinfo *pinfo = (struct pinfo *)info;
    struct master_db *db = db_ptr;
    struct lms_file_info finfo;
    unsigned int flags;
    int r, reply;

    r = _finfo_update(db, info, &finfo, &flags);
    if (r == 0)
        return r;

    if (_master_send_file(&pinfo->master, finfo, flags) != 0)
        return -1;

    r = _master_recv_reply(&pinfo->master, &pinfo->poll, &reply,
                           pinfo->common.lms->slave_timeout);
    if (r < 0) {
        _report_progress(info, &finfo, LMS_PROGRESS_STATUS_ERROR_COMM);
        return -2;
    } else if (r == 1) {
        fprintf(stderr, "ERROR: slave took too long, restart %d\n",
                pinfo->child);
        _report_progress(info, &finfo, LMS_PROGRESS_STATUS_KILLED);
        if (lms_restart_slave(pinfo, _slave_work) != 0)
            return -3;
        return 1;
    } else {
        if (reply < 0) {
            fprintf(stderr, "ERROR: pid=%d failed to parse \"%s\".\n",
                    getpid(), finfo.path);
            _report_progress(info, &finfo, LMS_PROGRESS_STATUS_ERROR_PARSE);
            return (-reply) << 8;
        } else {
            if (!finfo.dtime)
                _report_progress(info, &finfo, LMS_PROGRESS_STATUS_PROCESSED);
            else
                _report_progress(info, &finfo, LMS_PROGRESS_STATUS_DELETED);
            return reply;
        }
    }
}

static int
_check_row_single_process(void *db_ptr, struct cinfo *info)
{
    struct sinfo *sinfo = (struct sinfo *)info;
    struct single_process_db *db = db_ptr;
    struct lms_file_info finfo;
    unsigned int flags;
    int r;

    void **parser_match = sinfo->parser_match;
    lms_t *lms = info->lms;

    r = _finfo_update(db, info, &finfo, &flags);
    if (r == 0)
        return r;

    r = lms_db_update_file_info(db->update_file_info, &finfo, 0);
    if (r < 0)
        fprintf(stderr, "ERROR: could not update path in DB\n");
    else if (flags & COMM_FINFO_FLAG_OUTDATED) {
        int used;

        used = lms_parsers_check_using(lms, parser_match, &finfo);
        if (!used)
            r = 0;
        else {
            r = lms_parsers_run(lms, db->handle, parser_match, &finfo);
            if (r < 0) {
                fprintf(stderr, "ERROR: pid=%d failed to parse \"%s\".\n",
                        getpid(), finfo.path);
                lms_db_delete_file_info(db->delete_file_info, &finfo);
            }
        }
    }

    if (r < 0) {
        _report_progress(info, &finfo, LMS_PROGRESS_STATUS_ERROR_PARSE);
        return (-r) << 8;
    } else {
        sinfo->commit_counter++;
        if (sinfo->commit_counter > lms->commit_interval) {
            lms_db_end_transaction(db->transaction_commit);
            lms_db_begin_transaction(db->transaction_begin);
            sinfo->commit_counter = 0;
        }

        if (!finfo.dtime)
            _report_progress(info, &finfo, LMS_PROGRESS_STATUS_PROCESSED);
        else
            _report_progress(info, &finfo, LMS_PROGRESS_STATUS_DELETED);
        return r;
    }
}

static int
_init_sync_wait(struct pinfo *pinfo, int restart)
{
    int r, reply;

    do {
        r = _master_recv_reply(&pinfo->master, &pinfo->poll, &reply,
                               pinfo->common.lms->slave_timeout);
        if (r < 0)
            return -1;
        else if (r == 1 && restart) {
            fprintf(stderr, "ERROR: slave took too long, restart %d\n",
                    pinfo->child);
            if (lms_restart_slave(pinfo, _slave_work) != 0)
                return -2;
        }
    } while (r != 0 && restart);

    return r;
}

static int
_master_dummy_send_finish(const struct fds *master)
{
    return 0;
}

static int
_db_files_loop(void *db_ptr, struct cinfo *info, check_row_callback_t check_row)
{
    struct master_db *db = db_ptr;
    lms_t *lms = info->lms;
    int r;

    do {
        r = sqlite3_step(db->get_files);
        if (r == SQLITE_ROW) {
            if (check_row(db_ptr, info) < 0) {
                fprintf(stderr, "ERROR: could not check row.\n");
                return -1;
            }
        } else if (r != SQLITE_DONE) {
            fprintf(stderr, "ERROR: could not begin transaction: %s\n",
                    sqlite3_errmsg(db->handle));
            return -2;
        }
    } while (r != SQLITE_DONE && !lms->stop_processing);

    return 0;
}

static int
_check(struct pinfo *pinfo, int len, char *path)
{
    char query[PATH_SIZE + 2];
    struct master_db *db;
    int ret;

    db = _master_db_open(pinfo->common.lms->db_path);
    if (!db)
        return -1;

    memcpy(query, path, len);
    query[len] = '%';
    query[len + 1] = '\0';
    ret = lms_db_get_files(db->get_files, query, len + 1);
    if (ret != 0)
        goto end;

    if (lms_create_slave(pinfo, _slave_work) != 0) {
        ret = -2;
        goto end;
    }

    _init_sync_wait(pinfo, 1);

    ret = _db_files_loop(db, (struct cinfo *)pinfo, _check_row);

    _master_send_finish(&pinfo->master);
    _init_sync_wait(pinfo, 0);
    lms_finish_slave(pinfo, _master_dummy_send_finish);
  end:
    lms_db_reset_stmt(db->get_files);
    _master_db_close(db);

    return ret;
}

static int
_check_single_process(struct sinfo *sinfo, int len, char *path)
{
    struct single_process_db *db;
    char query[PATH_SIZE + 2];
    void **parser_match;
    lms_t *lms;
    int ret;

    lms = sinfo->common.lms;
    db = _single_process_db_open(lms->db_path);
    if (!db)
        return -1;

    memcpy(query, path, len);
    query[len] = '%';
    query[len + 1] = '\0';
    ret = lms_db_get_files(db->get_files, query, len + 1);
    if (ret != 0)
        goto end;

    if (lms_parsers_setup(lms, db->handle) != 0) {
        fprintf(stderr, "ERROR: could not setup parsers.\n");
        ret = -2;
        goto end;
    }

    if (lms_parsers_start(lms, db->handle) != 0) {
        fprintf(stderr, "ERROR: could not start parsers.\n");
        ret = -3;
        goto end;
    }

    if (lms->n_parsers < 1) {
        fprintf(stderr, "ERROR: no parser could be started, exit.\n");
        ret = -4;
        goto end;
    }

    parser_match = malloc(lms->n_parsers * sizeof(*parser_match));
    if (!parser_match) {
        perror("malloc");
        ret = -5;
        goto end;
    }

    sinfo->parser_match = parser_match;

    lms_db_begin_transaction(db->transaction_begin);

    ret = _db_files_loop(db, (struct cinfo *)sinfo, _check_row_single_process);

    free(parser_match);
    lms_db_end_transaction(db->transaction_commit);
  end:
    lms_parsers_finish(lms, db->handle);
    lms_db_reset_stmt(db->get_files);
    _single_process_db_close(db);

    return ret;
}

static int
_lms_check_check_valid(lms_t *lms, const char *path)
{
    if (!lms)
        return -1;

    if (!path)
        return -2;

    if (lms->is_processing) {
        fprintf(stderr, "ERROR: is already processing.\n");
        return -3;
    }

    if (!lms->parsers) {
        fprintf(stderr, "ERROR: no plugins registered.\n");
        return -4;
    }

    return 0;
}

/**
 * Check consistency of given directory.
 *
 * This will update media in the given directory or its children. If files
 * are missing, they'll be marked as deleted (dtime is set), if they were
 * marked as deleted and are now present, they are unmarked (dtime is unset).
 *
 * @param lms previously allocated Light Media Scanner instance.
 * @param top_path top directory to scan.
 *
 * @return On success 0 is returned.
 */
int
lms_check(lms_t *lms, const char *top_path)
{
    char path[PATH_SIZE];
    struct pinfo pinfo;
    int r;

    r = _lms_check_check_valid(lms, top_path);
    if (r < 0)
        return r;

    pinfo.common.lms = lms;

    if (lms_create_pipes(&pinfo) != 0) {
        r = -5;
        goto end;
    }

    if (realpath(top_path, path) == NULL) {
        perror("realpath");
        r = -6;
        goto close_pipes;
    }

    lms->is_processing = 1;
    lms->stop_processing = 0;
    r = _check(&pinfo, strlen(path), path);
    lms->is_processing = 0;
    lms->stop_processing = 0;

  close_pipes:
    lms_close_pipes(&pinfo);
  end:
    return r;
}

/**
 * Check consistency of given directory *without fork()-ing* into child process.
 *
 * This will update media in the given directory or its children. If files
 * are missing, they'll be marked as deleted (dtime is set), if they were
 * marked as deleted and are now present, they are unmarked (dtime is unset).
 * Note that if a parser hangs in the check process, this call will also hang.
 *
 * @param lms previously allocated Light Media Scanner instance.
 * @param top_path top directory to scan.
 *
 * @return On success 0 is returned.
 */
int
lms_check_single_process(lms_t *lms, const char *top_path)
{
    char path[PATH_SIZE];
    struct sinfo sinfo;
    int r;

    r = _lms_check_check_valid(lms, top_path);
    if (r < 0)
        return r;

    sinfo.common.lms = lms;
    sinfo.commit_counter = 0;

    if (realpath(top_path, path) == NULL) {
        perror("realpath");
        return -6;
    }

    lms->is_processing = 1;
    lms->stop_processing = 0;
    r = _check_single_process(&sinfo, strlen(path), path);
    lms->is_processing = 0;
    lms->stop_processing = 0;

    return r;
}
