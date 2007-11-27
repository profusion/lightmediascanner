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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <dlfcn.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lightmediascanner.h"
#include "lightmediascanner_plugin.h"
#include "lightmediascanner_db_private.h"

#define PATH_SIZE PATH_MAX
#define DEFAULT_SLAVE_TIMEOUT 1000
#define DEFAULT_COMMIT_INTERVAL 100

struct fds {
    int r;
    int w;
};

/* info to be carried along lms_process() */
struct pinfo {
    struct fds master;
    struct fds slave;
    struct pollfd poll;
    lms_t *lms;
    pid_t child;
};

struct parser {
    lms_plugin_t *plugin;
    void *dl_handle;
    char *so_path;
};

struct lms {
    struct parser *parsers;
    int n_parsers;
    char *db_path;
    int slave_timeout;
    unsigned int commit_interval;
    unsigned int is_processing:1;
};

struct db {
    sqlite3 *handle;
    sqlite3_stmt *transaction_begin;
    sqlite3_stmt *transaction_commit;
    sqlite3_stmt *transaction_rollback;
    sqlite3_stmt *get_file_info;
    sqlite3_stmt *insert_file_info;
    sqlite3_stmt *update_file_info;
    sqlite3_stmt *delete_file_info;
    sqlite3_stmt *clear_file_dtime;
};

/***********************************************************************
 * Master-Slave communication.
 ***********************************************************************/

static int
_master_send_path(const struct fds *master, int plen, int dlen, const char *p)
{
    int lengths[2];

    lengths[0] = plen;
    lengths[1] = dlen;

    if (write(master->w, lengths, sizeof(lengths)) < 0) {
        perror("write");
        return -1;
    }

    if (write(master->w, p, plen) < 0) {
        perror("write");
        return -1;
    }

    return 0;
}

static int
_master_send_finish(const struct fds *master)
{
    const int lengths[2] = {-1, -1};

    if (write(master->w, lengths, sizeof(lengths)) < 0) {
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
_slave_recv_path(const struct fds *slave, int *plen, int *dlen, char *path)
{
    int lengths[2], r;

    r = read(slave->r, lengths, sizeof(lengths));
    if (r != sizeof(lengths)) {
        perror("read");
        return -1;
    }
    *plen = lengths[0];
    *dlen = lengths[1];

    if (*plen == -1)
        return 0;

    if (*plen > PATH_SIZE) {
        fprintf(stderr, "ERROR: path too long (%d/%d)\n", *plen, PATH_SIZE);
        return -2;
    }

    r = read(slave->r, path, *plen);
    if (r != *plen) {
        fprintf(stderr, "ERROR: could not read whole path %d/%d\n", r, *plen);
        return -3;
    }

    path[*plen] = 0;
    return 0;
}


/***********************************************************************
 * Slave-side.
 ***********************************************************************/

static int
_db_create_tables_if_required(sqlite3 *db)
{
    char *errmsg;
    int r;

    errmsg = NULL;
    r = sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS files ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "path TEXT NOT NULL UNIQUE, "
                     "mtime INTEGER NOT NULL, "
                     "dtime INTEGER NOT NULL"
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
_db_compile_all_stmts(struct db *db)
{
    db->transaction_begin = lms_db_compile_stmt(db->handle,
        "BEGIN TRANSACTION");
    if (!db->transaction_begin)
        return -1;

    db->transaction_commit = lms_db_compile_stmt(db->handle,
        "COMMIT");
    if (!db->transaction_commit)
        return -2;

    db->transaction_rollback = lms_db_compile_stmt(db->handle,
        "ROLLBACK");
    if (!db->transaction_rollback)
        return -3;

    db->get_file_info = lms_db_compile_stmt(db->handle,
        "SELECT id, mtime, dtime FROM files WHERE path = ?");
    if (!db->get_file_info)
        return -4;

    db->insert_file_info = lms_db_compile_stmt(db->handle,
        "INSERT INTO files (path, mtime, dtime) VALUES(?, ?, ?)");
    if (!db->insert_file_info)
        return -5;

    db->update_file_info = lms_db_compile_stmt(db->handle,
        "UPDATE files SET mtime = ?, dtime = ? WHERE id = ?");
    if (!db->update_file_info)
        return -6;

    db->delete_file_info = lms_db_compile_stmt(db->handle,
        "DELETE FROM files WHERE id = ?");
    if (!db->delete_file_info)
        return -6;

    db->clear_file_dtime = lms_db_compile_stmt(db->handle,
        "UPDATE files SET dtime = 0 WHERE id = ?");
    if (!db->clear_file_dtime)
        return -7;

    return 0;
}

static struct db *
_db_open(const char *db_path)
{
    struct db *db;

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

    if (_db_create_tables_if_required(db->handle) != 0) {
        fprintf(stderr, "ERROR: could not setup tables and indexes.\n");
        goto error;
    }

    return db;

  error:
    sqlite3_close(db->handle);
    free(db);
    return NULL;
}

static int
_db_close(struct db *db)
{
    if (db->transaction_begin)
        lms_db_finalize_stmt(db->transaction_begin, "transaction_begin");

    if (db->transaction_commit)
        lms_db_finalize_stmt(db->transaction_commit, "transaction_commit");

    if (db->transaction_rollback)
        lms_db_finalize_stmt(db->transaction_rollback, "transaction_rollback");

    if (db->get_file_info)
        lms_db_finalize_stmt(db->get_file_info, "get_file_info");

    if (db->insert_file_info)
        lms_db_finalize_stmt(db->insert_file_info, "insert_file_info");

    if (db->update_file_info)
        lms_db_finalize_stmt(db->update_file_info, "update_file_info");

    if (db->delete_file_info)
        lms_db_finalize_stmt(db->delete_file_info, "delete_file_info");

    if (db->clear_file_dtime)
        lms_db_finalize_stmt(db->clear_file_dtime, "clear_file_dtime");

    if (sqlite3_close(db->handle) != SQLITE_OK) {
        fprintf(stderr, "ERROR: clould not close DB: %s\n",
                sqlite3_errmsg(db->handle));
        return -1;
    }
    free(db);

    return 0;
}

static int
_db_begin_transaction(struct db *db)
{
    sqlite3_stmt *stmt;
    int r, ret;

    stmt = db->transaction_begin;

    ret = 0;
    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not begin transaction: %s\n",
                sqlite3_errmsg(db->handle));
        ret = -1;
    }

    r = sqlite3_reset(stmt);
    if (r != SQLITE_OK)
        fprintf(stderr, "ERROR: could not reset SQL statement: %s\n",
                sqlite3_errmsg(db->handle));

    return ret;
}

static int
_db_end_transaction(struct db *db)
{
    sqlite3_stmt *stmt;
    int r, ret;

    stmt = db->transaction_commit;

    ret = 0;
    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not end transaction: %s\n",
                sqlite3_errmsg(db->handle));
        ret = -1;
    }

    r = sqlite3_reset(stmt);
    if (r != SQLITE_OK)
        fprintf(stderr, "ERROR: could not reset SQL statement: %s\n",
                sqlite3_errmsg(db->handle));

    return ret;
}

static int
_db_get_file_info(struct db *db, struct lms_file_info *finfo)
{
    sqlite3_stmt *stmt;
    int r, ret;

    stmt = db->get_file_info;

    ret = lms_db_bind_text(stmt, 1, finfo->path, finfo->path_len);
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
                sqlite3_errmsg(db->handle));
        ret = -2;
        goto done;
    }

    finfo->id = sqlite3_column_int64(stmt, 0);
    finfo->mtime = sqlite3_column_int(stmt, 1);
    finfo->dtime = sqlite3_column_int(stmt, 2);
    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

static int
_db_update_file_info(struct db *db, struct lms_file_info *finfo)
{
    sqlite3_stmt *stmt;
    int r, ret;

    stmt = db->update_file_info;

    ret = lms_db_bind_int(stmt, 1, finfo->mtime);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 2, finfo->dtime);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 3, finfo->id);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not update file info: %s\n",
                sqlite3_errmsg(db->handle));
        ret = -4;
        goto done;
    }

    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

static int
_db_insert_file_info(struct db *db, struct lms_file_info *finfo)
{
    sqlite3_stmt *stmt;
    int r, ret;

    stmt = db->insert_file_info;

    ret = lms_db_bind_text(stmt, 1, finfo->path, finfo->path_len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 2, finfo->mtime);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 3, finfo->dtime);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not insert file info: %s\n",
                sqlite3_errmsg(db->handle));
        ret = -4;
        goto done;
    }

    finfo->id = sqlite3_last_insert_rowid(db->handle);
    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

static int
_db_delete_file_info(struct db *db, struct lms_file_info *finfo)
{
    sqlite3_stmt *stmt;
    int r, ret;

    stmt = db->delete_file_info;

    ret = lms_db_bind_int64(stmt, 1, finfo->id);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not delete file info: %s\n",
                sqlite3_errmsg(db->handle));
        ret = -2;
        goto done;
    }
    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

static int
_db_clear_file_dtime(struct db *db, struct lms_file_info *finfo)
{
    sqlite3_stmt *stmt;
    int r, ret;

    stmt = db->clear_file_dtime;

    ret = lms_db_bind_int64(stmt, 1, finfo->id);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not clear file dtime: %s\n",
                sqlite3_errmsg(db->handle));
        ret = -2;
        goto done;
    }

    finfo->dtime = 0;
    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

static int
_retrieve_file_status(struct db *db, struct lms_file_info *finfo)
{
    struct stat st;
    int r;

    if (stat(finfo->path, &st) != 0) {
        perror("stat");
        return -1;
    }

    r = _db_get_file_info(db, finfo);
    if (r == 0) {
        if (st.st_mtime <= finfo->mtime)
            return 0;
        else {
            finfo->mtime = st.st_mtime;
            return 1;
        }
    } else if (r == 1) {
        finfo->mtime = st.st_mtime;
        return 1;
    } else
        return -2;
}

static int _parser_del_int(lms_t *lms, int i);

static int
_parsers_setup(lms_t *lms, struct db *db)
{
    int i;

    for (i = 0; i < lms->n_parsers; i++) {
        lms_plugin_t *plugin;
        int r;

        plugin = lms->parsers[i].plugin;
        r = plugin->setup(plugin, db->handle);
        if (r != 0) {
            fprintf(stderr, "ERROR: parser \"%s\" failed to setup: %d.\n",
                    plugin->name, r);
            plugin->finish(plugin, db->handle);
            _parser_del_int(lms, i);
            i--; /* cancel i++ */
        }
    }

    return 0;
}

static int
_parsers_start(lms_t *lms, struct db *db)
{
    int i;

    for (i = 0; i < lms->n_parsers; i++) {
        lms_plugin_t *plugin;
        int r;

        plugin = lms->parsers[i].plugin;
        r = plugin->start(plugin, db->handle);
        if (r != 0) {
            fprintf(stderr, "ERROR: parser \"%s\" failed to start: %d.\n",
                    plugin->name, r);
            plugin->finish(plugin, db->handle);
            _parser_del_int(lms, i);
            i--; /* cancel i++ */
        }
    }

    return 0;
}

static int
_parsers_finish(lms_t *lms, struct db *db)
{
    int i;

    for (i = 0; i < lms->n_parsers; i++) {
        lms_plugin_t *plugin;
        int r;

        plugin = lms->parsers[i].plugin;
        r = plugin->finish(plugin, db->handle);
        if (r != 0)
            fprintf(stderr, "ERROR: parser \"%s\" failed to finish: %d.\n",
                    plugin->name, r);
    }

    return 0;
}

static int
_parsers_check_using(lms_t *lms, void **parser_match, struct lms_file_info *finfo)
{
    int used, i;

    used = 0;
    for (i = 0; i < lms->n_parsers; i++) {
        lms_plugin_t *plugin;
        void *r;

        plugin = lms->parsers[i].plugin;
        r = plugin->match(plugin, finfo->path, finfo->path_len, finfo->base);
        parser_match[i] = r;
        if (r)
            used = 1;
    }

    return used;
}

static int
_parsers_run(lms_t *lms, struct db *db, void **parser_match, struct lms_file_info *finfo)
{
    int i, failed, available;

    failed = 0;
    available = 0;
    for (i = 0; i < lms->n_parsers; i++) {
        lms_plugin_t *plugin;

        plugin = lms->parsers[i].plugin;
        if (parser_match[i]) {
            int r;

            available++;
            r = plugin->parse(plugin, db->handle, finfo, parser_match[i]);
            if (r != 0)
                failed++;
        }
    }

    if (!failed)
        return 0;
    else if (failed == available)
        return -1;
    else
        return 1; /* non critical */
}

static int
_slave_work(lms_t *lms, struct fds *fds)
{
    int r, len, base, counter;
    char path[PATH_SIZE];
    void **parser_match;
    struct db *db;

    db = _db_open(lms->db_path);
    if (!db)
        return -1;

    if (_parsers_setup(lms, db) != 0) {
        fprintf(stderr, "ERROR: could not setup parsers.\n");
        r = -2;
        goto end;
    }

    if (_db_compile_all_stmts(db) != 0) {
        fprintf(stderr, "ERROR: could not compile statements.\n");
        r = -3;
        goto end;
    }

    if (_parsers_start(lms, db) != 0) {
        fprintf(stderr, "ERROR: could not start parsers.\n");
        r = -4;
        goto end;
    }
    if (lms->n_parsers < 1) {
        fprintf(stderr, "ERROR: no parser could be started, exit.\n");
        r = -5;
        goto end;
    }

    parser_match = malloc(lms->n_parsers * sizeof(*parser_match));
    if (!parser_match) {
        perror("malloc");
        r = -6;
        goto end;
    }

    counter = 0;
    _db_begin_transaction(db);

    while (((r = _slave_recv_path(fds, &len, &base, path)) == 0) && len > 0) {
        struct lms_file_info finfo;
        int used, r;

        finfo.path = path;
        finfo.path_len = len;
        finfo.base = base;

        r = _retrieve_file_status(db, &finfo);
        if (r == 0) {
            if (finfo.dtime)
                _db_clear_file_dtime(db, &finfo);
            goto inform_end;
        } else if (r < 0) {
            fprintf(stderr, "ERROR: could not detect file status.\n");
            goto inform_end;
        }

        used = _parsers_check_using(lms, parser_match, &finfo);
        if (!used)
            goto inform_end;

        finfo.dtime = 0;
        if (finfo.id > 0)
            r = _db_update_file_info(db, &finfo);
        else
            r = _db_insert_file_info(db, &finfo);
        if (r < 0) {
            fprintf(stderr, "ERROR: could not register path in DB\n");
            goto inform_end;
        }

        r = _parsers_run(lms, db, parser_match, &finfo);
        if (r < 0) {
            fprintf(stderr, "ERROR: pid=%d failed to parse \"%s\".\n",
                    getpid(), finfo.path);
            _db_delete_file_info(db, &finfo);
        }

      inform_end:
        _slave_send_reply(fds, r);
        counter++;
        if (counter > lms->commit_interval) {
            _db_end_transaction(db);
            _db_begin_transaction(db);
            counter = 0;
        }
    }

    free(parser_match);
    _db_end_transaction(db);
  end:
    _parsers_finish(lms, db);
    _db_close(db);

    return r;
}


/***********************************************************************
 * Master-side.
 ***********************************************************************/

static int
_consume_garbage(struct pollfd *pfd)
{
    int r;

    while ((r = poll(pfd, 1, 0)) > 0) {
        if (pfd->revents & (POLLERR | POLLHUP | POLLNVAL))
            return 0;
        else if (pfd->revents & POLLIN) {
            char c;

            read(pfd->fd, &c, sizeof(c));
        }
    }

    return r;
}

static int
_close_fds(struct fds *fds)
{
    int r;

    r = 0;
    if (close(fds->r) != 0) {
        r--;
        perror("close");
    }

    if (close(fds->w) != 0) {
        r--;
        perror("close");
    }

    return r;
}

static int
_close_pipes(struct pinfo *pinfo)
{
    int r;

    r = _close_fds(&pinfo->master);
    r += _close_fds(&pinfo->slave);

    return r;
}

static int
_create_pipes(struct pinfo *pinfo)
{
    int fds[2];

    if (pipe(fds) != 0) {
        perror("pipe");
        return -1;
    }
    pinfo->master.r = fds[0];
    pinfo->slave.w = fds[1];

    if (pipe(fds) != 0) {
        perror("pipe");
        close(pinfo->master.r);
        close(pinfo->slave.w);
        return -1;
    }
    pinfo->slave.r = fds[0];
    pinfo->master.w = fds[1];

    pinfo->poll.fd = pinfo->master.r;
    pinfo->poll.events = POLLIN;

    return 0;
}

static int
_create_slave(struct pinfo *pinfo)
{
    int r;

    pinfo->child = fork();
    if (pinfo->child == -1) {
        perror("fork");
        return -1;
    }

    if (pinfo->child > 0)
        return 0;

    _close_fds(&pinfo->master);
    nice(19);
    r = _slave_work(pinfo->lms, &pinfo->slave);
    lms_free(pinfo->lms);
    _exit(r);
    return r; /* shouldn't reach anyway... */
}

static int
_waitpid(pid_t pid)
{
    int status;
    pid_t r;

    r = waitpid(pid, &status, 0);
    if (r > -1)
        return 0;
    else
        perror("waitpid");

    return r;
}

static int
_finish_slave(struct pinfo *pinfo)
{
    int r;

    if (pinfo->child <= 0)
        return 0;

    r = _master_send_finish(&pinfo->master);
    if (r == 0)
        r = _waitpid(pinfo->child);
    else {
        r = kill(pinfo->child, SIGKILL);
        if (r < 0)
            perror("kill");
        else
            r =_waitpid(pinfo->child);
    }

    return r;
}

static int
_restart_slave(struct pinfo *pinfo)
{
    int status;

    if (waitpid(pinfo->child, &status, WNOHANG) > 0) {
        if (WIFEXITED(status)) {
            int code;

            code = WEXITSTATUS(status);
            if (code != 0) {
                fprintf(stderr, "ERROR: slave returned %d, exit.\n", code);
                pinfo->child = 0;
                return -1;
            }
        } else {
            if (WIFSIGNALED(status)) {
                int code;

                code = WTERMSIG(status);
                fprintf(stderr, "ERROR: slave was terminated by signal %d.\n",
                        code);
            }
            pinfo->child = 0;
            return -1;
        }
    }

    if (kill(pinfo->child, SIGKILL))
        perror("kill");

    if (waitpid(pinfo->child, &status, 0) < 0)
        perror("waitpid");

    _consume_garbage(&pinfo->poll);
    return _create_slave(pinfo);
}

static int
_strcat(int base, char *path, const char *name)
{
    int new_len, name_len;

    name_len = strlen(name);
    new_len = base + name_len;

    if (new_len >= PATH_SIZE) {
        path[base] = '\0';
        fprintf(stderr,
                "ERROR: path concatenation too long %d of %d "
                "available: \"%s\" + \"%s\"\n", new_len, PATH_SIZE,
                path, name);
        return -1;
    }

    memcpy(path + base, name, name_len + 1);

    return new_len;
}

static int
_process_file(struct pinfo *pinfo, int base, char *path, const char *name)
{
    int new_len, reply, r;

    new_len = _strcat(base, path, name);
    if (new_len < 0)
        return -1;

    if (_master_send_path(&pinfo->master, new_len, base, path) != 0)
        return -2;

    r = _master_recv_reply(&pinfo->master, &pinfo->poll, &reply,
                           pinfo->lms->slave_timeout);
    if (r < 0)
        return -3;
    else if (r == 1) {
        fprintf(stderr, "ERROR: slave took too long, restart %d\n",
                pinfo->child);
        if (_restart_slave(pinfo) != 0)
            return -4;
        return 1;
    } else {
        if (reply < 0) {
            /* XXX callback library users to inform error. */
            fprintf(stderr, "ERROR: pid=%d failed to parse \"%s\".\n",
                    getpid(), path);
            return (-reply) << 8;
        } else
            return reply;
    }
}

static int
_process_dir(struct pinfo *pinfo, int base, char *path, const char *name)
{
    DIR *dir;
    struct dirent *de;
    int new_len, r;

    new_len = _strcat(base, path, name);
    if (new_len < 0)
        return -1;
    else if (new_len + 1 >= PATH_SIZE) {
        fprintf(stderr, "ERROR: path too long\n");
        return 2;
    }

    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return 3;
    }

    path[new_len] = '/';
    new_len++;

    r = 0;
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.')
            continue;
        if (de->d_type == DT_REG) {
            if (_process_file(pinfo, new_len, path, de->d_name) < 0) {
                path[new_len - 1] = '\0';
                fprintf(stderr,
                        "ERROR: unrecoverable error parsing file, "
                        "exit \"%s\".\n", path);
                r = -4;
                goto end;
            }
        } else if (de->d_type == DT_DIR || de->d_type == DT_UNKNOWN) {
            if (_process_dir(pinfo, new_len, path, de->d_name) < 0) {
                path[new_len - 1] = '\0';
                fprintf(stderr,
                        "ERROR: unrecoverable error parsing dir, "
                        "exit \"%s\".\n", path);
                r = -5;
                goto end;
            }
        }
    }

  end:
    closedir(dir);
    return r;
}


/***********************************************************************
 * Plugins handling.
 ***********************************************************************/

static int
_parser_load(struct parser *p, const char *so_path)
{
    lms_plugin_t *(*plugin_open)(void);
    char *errmsg;

    memset(p, 0, sizeof(*p));

    p->dl_handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    errmsg = dlerror();
    if (errmsg) {
        fprintf(stderr, "ERROR: could not dlopen() %s\n", errmsg);
        return -1;
    }

    plugin_open = dlsym(p->dl_handle, "lms_plugin_open");
    errmsg = dlerror();
    if (errmsg) {
        fprintf(stderr, "ERROR: could not find plugin entry point %s\n",
                errmsg);
        return -2;
    }

    p->so_path = strdup(so_path);
    if (!p->so_path) {
        perror("strdup");
        return -3;
    }

    p->plugin = plugin_open();
    if (!p->plugin) {
        fprintf(stderr, "ERROR: plugin \"%s\" failed to init.\n", so_path);
        return -4;
    }

    return 0;
}

static int
_parser_unload(struct parser *p)
{
    int r;

    r = 0;
    if (p->plugin) {
        if (p->plugin->close(p->plugin) != 0) {
            fprintf(stderr, "ERROR: plugin \"%s\" failed to deinit.\n",
                    p->so_path);
            r -= 1;
        }
    }

    if (p->dl_handle) {
        char *errmsg;

        dlclose(p->dl_handle);
        errmsg = dlerror();
        if (errmsg) {
            fprintf(stderr, "ERROR: could not dlclose() plugin \"%s\": %s\n",
                    errmsg, p->so_path);
            r -= 1;
        }
    }

    if (p->so_path)
        free(p->so_path);

    return r;
}


/***********************************************************************
 * Public API.
 ***********************************************************************/
lms_t *
lms_new(const char *db_path)
{
    lms_t *lms;

    lms = calloc(1, sizeof(lms_t));
    if (!lms) {
        perror("calloc");
        return NULL;
    }

    lms->commit_interval = DEFAULT_COMMIT_INTERVAL;
    lms->slave_timeout = DEFAULT_SLAVE_TIMEOUT;
    lms->db_path = strdup(db_path);
    if (!lms->db_path) {
        perror("strdup");
        free(lms);
        return NULL;
    }

    return lms;
}

int
lms_free(lms_t *lms)
{
    int i;

    if (!lms)
        return 0;

    if (lms->is_processing)
        return -1;

    if (lms->parsers) {
        for (i = 0; i < lms->n_parsers; i++)
            _parser_unload(lms->parsers + i);

        free(lms->parsers);
    }

    free(lms->db_path);
    free(lms);
    return 0;
}

lms_plugin_t *
lms_parser_add(lms_t *lms, const char *so_path)
{
    struct parser *parser;

    if (!lms)
        return NULL;

    if (!so_path)
        return NULL;

    if (lms->is_processing) {
        fprintf(stderr, "ERROR: do not add parsers while it's processing.\n");
        return NULL;
    }

    lms->parsers = realloc(lms->parsers,
                           (lms->n_parsers + 1) * sizeof(struct parser));
    if (!lms->parsers) {
        perror("realloc");
        return NULL;
    }

    parser = lms->parsers + lms->n_parsers;
    if (_parser_load(parser, so_path) != 0) {
        _parser_unload(parser);
        return NULL;
    }

    lms->n_parsers++;
    return parser->plugin;
}

lms_plugin_t *
lms_parser_find_and_add(lms_t *lms, const char *name)
{
    char so_path[PATH_MAX];

    if (!lms)
        return NULL;
    if (!name)
        return NULL;

    snprintf(so_path, sizeof(so_path), "%s/%s.so", PLUGINSDIR, name);
    return lms_parser_add(lms, so_path);
}

static int
_parser_del_int(lms_t *lms, int i)
{
    struct parser *parser;

    parser = lms->parsers + i;
    _parser_unload(parser);
    lms->n_parsers--;

    if (lms->n_parsers == 0) {
        free(lms->parsers);
        lms->parsers = NULL;
        return 0;
    } else {
        int dif;

        dif = lms->n_parsers - i;
        if (dif)
            lms->parsers = memmove(parser, parser + 1,
                                   dif * sizeof(struct parser));

        lms->parsers = realloc(lms->parsers,
                               lms->n_parsers * sizeof(struct parser));
        if (!lms->parsers) {
            lms->n_parsers = 0;
            return -1;
        }

        return 0;
    }
}

int
lms_parser_del(lms_t *lms, lms_plugin_t *handle)
{
    int i;

    if (!lms)
        return -1;
    if (!handle)
        return -2;
    if (!lms->parsers)
        return -3;
    if (lms->is_processing) {
        fprintf(stderr, "ERROR: do not del parsers while it's processing.\n");
        return -4;
    }

    for (i = 0; i < lms->n_parsers; i++)
        if (lms->parsers[i].plugin == handle)
            return _parser_del_int(lms, i);

    return -3;
}

int
lms_process(lms_t *lms, const char *top_path)
{
    struct pinfo pinfo;
    int r, len;
    char path[PATH_SIZE], *bname;

    if (!lms) {
        r = -1;
        goto end;
    }

    if (!top_path) {
        r = -2;
        goto end;
    }

    if (lms->is_processing) {
        fprintf(stderr, "ERROR: is already processing.\n");
        r = -3;
        goto end;
    }

    if (!lms->parsers) {
        fprintf(stderr, "ERROR: no plugins registered.\n");
        r = -4;
        goto end;
    }

    pinfo.lms = lms;

    if (_create_pipes(&pinfo) != 0) {
        r = -5;
        goto end;
    }

    if (_create_slave(&pinfo) != 0) {
        r = -6;
        goto close_pipes;
    }

    if (realpath(top_path, path) == NULL) {
        perror("realpath");
        r = -7;
        goto finish_slave;
    }

    /* search '/' backwards, split dirname and basename, note realpath usage */
    len = strlen(path);
    for (; len >= 0 && path[len] != '/'; len--);
    len++;
    bname = strdup(path + len);
    if (bname == NULL) {
        perror("strdup");
        r = -8;
        goto finish_slave;
    }

    lms->is_processing = 1;
    r = _process_dir(&pinfo, len, path, bname);
    lms->is_processing = 0;
    free(bname);

  finish_slave:
    _finish_slave(&pinfo);
  close_pipes:
    _close_pipes(&pinfo);
  end:
    return r;
}

int
lms_is_processing(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_is_processing(NULL)\n");
        return -1;
    }

    return lms->is_processing;
}

const char *
lms_get_db_path(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_get_db_path(NULL)\n");
        return NULL;
    }

    return lms->db_path;
}

int
lms_get_slave_timeout(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_get_slave_timeout(NULL)\n");
        return -1;
    }

    return lms->slave_timeout;
}

void lms_set_slave_timeout(lms_t *lms, int ms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_set_slave_timeout(NULL, %d)\n", ms);
        return;
    }

    lms->slave_timeout = ms;
}

unsigned int
lms_get_commit_interval(const lms_t *lms)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_get_commit_interval(NULL)\n");
        return (unsigned int)-1;
    }

    return lms->commit_interval;
}

void
lms_set_commit_interval(lms_t *lms, unsigned int transactions)
{
    if (!lms) {
        fprintf(stderr, "ERROR: lms_set_commit_interval(NULL, %u)\n",
                transactions);
        return;
    }

    lms->commit_interval = transactions;
}
