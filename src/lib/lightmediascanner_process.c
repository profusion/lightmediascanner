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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * @author Gustavo Sverzut Barbieri <gustavo.barbieri@openbossa.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lightmediascanner.h"
#include "lightmediascanner_private.h"
#include "lightmediascanner_db_private.h"

struct db {
    sqlite3 *handle;
    sqlite3_stmt *transaction_begin;
    sqlite3_stmt *transaction_commit;
    sqlite3_stmt *get_file_info;
    sqlite3_stmt *insert_file_info;
    sqlite3_stmt *update_file_info;
    sqlite3_stmt *delete_file_info;
    sqlite3_stmt *set_file_dtime;
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
_db_compile_all_stmts(struct db *db)
{
    sqlite3 *handle;

    handle = db->handle;
    db->transaction_begin = lms_db_compile_stmt_begin_transaction(handle);
    if (!db->transaction_begin)
        return -1;

    db->transaction_commit = lms_db_compile_stmt_end_transaction(handle);
    if (!db->transaction_commit)
        return -2;

    db->get_file_info = lms_db_compile_stmt_get_file_info(handle);
    if (!db->get_file_info)
        return -4;

    db->insert_file_info = lms_db_compile_stmt_insert_file_info(handle);
    if (!db->insert_file_info)
        return -5;

    db->update_file_info = lms_db_compile_stmt_update_file_info(handle);
    if (!db->update_file_info)
        return -6;

    db->delete_file_info = lms_db_compile_stmt_delete_file_info(handle);
    if (!db->delete_file_info)
        return -6;

    db->set_file_dtime = lms_db_compile_stmt_set_file_dtime(handle);
    if (!db->set_file_dtime)
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

    if (lms_db_create_core_tables_if_required(db->handle) != 0) {
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

    if (db->get_file_info)
        lms_db_finalize_stmt(db->get_file_info, "get_file_info");

    if (db->insert_file_info)
        lms_db_finalize_stmt(db->insert_file_info, "insert_file_info");

    if (db->update_file_info)
        lms_db_finalize_stmt(db->update_file_info, "update_file_info");

    if (db->delete_file_info)
        lms_db_finalize_stmt(db->delete_file_info, "delete_file_info");

    if (db->set_file_dtime)
        lms_db_finalize_stmt(db->set_file_dtime, "set_file_dtime");

    if (sqlite3_close(db->handle) != SQLITE_OK) {
        fprintf(stderr, "ERROR: clould not close DB: %s\n",
                sqlite3_errmsg(db->handle));
        return -1;
    }
    free(db);

    return 0;
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

    r = lms_db_get_file_info(db->get_file_info, finfo);
    if (r == 0) {
        if (st.st_mtime <= finfo->mtime && finfo->size == st.st_size)
            return 0;
        else {
            finfo->mtime = st.st_mtime;
            finfo->size = st.st_size;
            return 1;
        }
    } else if (r == 1) {
        finfo->mtime = st.st_mtime;
        finfo->size = st.st_size;
        return 1;
    } else
        return -2;
}

static void
_ctxt_init(struct lms_context *ctxt, const lms_t *lms, sqlite3 *db)
{
    ctxt->cs_conv = lms->cs_conv;
    ctxt->db = db;
}

int
lms_parsers_setup(lms_t *lms, sqlite3 *db)
{
    struct lms_context ctxt;
    int i;

    _ctxt_init(&ctxt, lms, db);

    for (i = 0; i < lms->n_parsers; i++) {
        lms_plugin_t *plugin;
        int r;

        plugin = lms->parsers[i].plugin;
        r = plugin->setup(plugin, &ctxt);
        if (r != 0) {
            fprintf(stderr, "ERROR: parser \"%s\" failed to setup: %d.\n",
                    plugin->name, r);
            plugin->finish(plugin, &ctxt);
            lms_parser_del_int(lms, i);
            i--; /* cancel i++ */
        }
    }

    return 0;
}

int
lms_parsers_start(lms_t *lms, sqlite3 *db)
{
    struct lms_context ctxt;
    int i;

    _ctxt_init(&ctxt, lms, db);

    for (i = 0; i < lms->n_parsers; i++) {
        lms_plugin_t *plugin;
        int r;

        plugin = lms->parsers[i].plugin;
        r = plugin->start(plugin, &ctxt);
        if (r != 0) {
            fprintf(stderr, "ERROR: parser \"%s\" failed to start: %d.\n",
                    plugin->name, r);
            plugin->finish(plugin, &ctxt);
            lms_parser_del_int(lms, i);
            i--; /* cancel i++ */
        }
    }

    return 0;
}

int
lms_parsers_finish(lms_t *lms, sqlite3 *db)
{
    struct lms_context ctxt;
    int i;

    _ctxt_init(&ctxt, lms, db);

    for (i = 0; i < lms->n_parsers; i++) {
        lms_plugin_t *plugin;
        int r;

        plugin = lms->parsers[i].plugin;
        r = plugin->finish(plugin, &ctxt);
        if (r != 0)
            fprintf(stderr, "ERROR: parser \"%s\" failed to finish: %d.\n",
                    plugin->name, r);
    }

    return 0;
}

int
lms_parsers_check_using(lms_t *lms, void **parser_match, struct lms_file_info *finfo)
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

int
lms_parsers_run(lms_t *lms, sqlite3 *db, void **parser_match, struct lms_file_info *finfo)
{
    struct lms_context ctxt;
    int i, failed, available;

    _ctxt_init(&ctxt, lms, db);

    failed = 0;
    available = 0;
    for (i = 0; i < lms->n_parsers; i++) {
        lms_plugin_t *plugin;

        plugin = lms->parsers[i].plugin;
        if (parser_match[i]) {
            int r;

            available++;
            r = plugin->parse(plugin, &ctxt, finfo, parser_match[i]);
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
_db_and_parsers_setup(lms_t *lms, struct db **db_ret, void ***parser_match_ret)
{
    void **parser_match;
    struct db *db;
    int r = 0;

    db = _db_open(lms->db_path);
    if (!db) {
        r = -1;
        return r;
    }

    if (lms_parsers_setup(lms, db->handle) != 0) {
        fprintf(stderr, "ERROR: could not setup parsers.\n");
        r = -2;
        goto err;
    }

    if (_db_compile_all_stmts(db) != 0) {
        fprintf(stderr, "ERROR: could not compile statements.\n");
        r = -3;
        goto err;
    }

    if (lms_parsers_start(lms, db->handle) != 0) {
        fprintf(stderr, "ERROR: could not start parsers.\n");
        r = -4;
        goto err;
    }
    if (lms->n_parsers < 1) {
        fprintf(stderr, "ERROR: no parser could be started, exit.\n");
        r = -5;
        goto err;
    }

    parser_match = malloc(lms->n_parsers * sizeof(*parser_match));
    if (!parser_match) {
        perror("malloc");
        r = -6;
        goto err;
    }

    *parser_match_ret = parser_match;
    *db_ret = db;
    return r;

  err:
    lms_parsers_finish(lms, db->handle);
    _db_close(db);
    return r;
}

static int
_db_and_parsers_process_file(lms_t *lms, struct db *db, void **parser_match, char *path, int path_len, int path_base)
{
    struct lms_file_info finfo;
    int used, r;

    finfo.path = path;
    finfo.path_len = path_len;
    finfo.base = path_base;

    r = _retrieve_file_status(db, &finfo);
    if (r == 0) {
        if (finfo.dtime) {
            finfo.dtime = 0;
            lms_db_set_file_dtime(db->set_file_dtime, &finfo);
        }
        return r;
    } else if (r < 0) {
        fprintf(stderr, "ERROR: could not detect file status.\n");
        return r;
    }

    used = lms_parsers_check_using(lms, parser_match, &finfo);
    if (!used)
        return r;

    finfo.dtime = 0;
    if (finfo.id > 0)
        r = lms_db_update_file_info(db->update_file_info, &finfo);
    else
        r = lms_db_insert_file_info(db->insert_file_info, &finfo);
    if (r < 0) {
        fprintf(stderr, "ERROR: could not register path in DB\n");
        return r;
    }

    r = lms_parsers_run(lms, db->handle, parser_match, &finfo);
    if (r < 0) {
        fprintf(stderr, "ERROR: pid=%d failed to parse \"%s\".\n",
                getpid(), finfo.path);
        lms_db_delete_file_info(db->delete_file_info, &finfo);
    }

    return r;
}

static int
_slave_work(lms_t *lms, struct fds *fds)
{
    int r, len, base, counter;
    char path[PATH_SIZE];
    void **parser_match;
    struct db *db;

    r = _db_and_parsers_setup(lms, &db, &parser_match);
    if (r < 0)
        return r;

    counter = 0;
    lms_db_begin_transaction(db->transaction_begin);

    while (((r = _slave_recv_path(fds, &len, &base, path)) == 0) && len > 0) {
        r = _db_and_parsers_process_file(
            lms, db, parser_match, path, len, base);

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
    lms_parsers_finish(lms, db->handle);
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

int
lms_close_pipes(struct pinfo *pinfo)
{
    int r;

    r = _close_fds(&pinfo->master);
    r += _close_fds(&pinfo->slave);

    return r;
}

int
lms_create_pipes(struct pinfo *pinfo)
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

int
lms_create_slave(struct pinfo *pinfo, int (*work)(lms_t *lms, struct fds *fds))
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
    r = work(pinfo->lms, &pinfo->slave);
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

int
lms_finish_slave(struct pinfo *pinfo, int (*finish)(const struct fds *fds))
{
    int r;

    if (pinfo->child <= 0)
        return 0;

    r = finish(&pinfo->master);
    if (r == 0)
        r = _waitpid(pinfo->child);
    else {
        r = kill(pinfo->child, SIGKILL);
        if (r < 0)
            perror("kill");
        else
            r =_waitpid(pinfo->child);
    }
    pinfo->child = 0;

    return r;
}

int
lms_restart_slave(struct pinfo *pinfo, int (*work)(lms_t *lms, struct fds *fds))
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
    return lms_create_slave(pinfo, work);
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

static inline void
_report_progress(struct pinfo *pinfo, const char *path, int path_len, lms_progress_status_t status)
{
    lms_progress_callback_t cb;
    lms_t *lms = pinfo->lms;

    cb = lms->progress.cb;
    if (!cb)
        return;

    cb(lms, path, path_len, status, lms->progress.data);
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
    if (r < 0) {
        _report_progress(pinfo, path, new_len, LMS_PROGRESS_STATUS_ERROR_COMM);
        return -3;
    } else if (r == 1) {
        fprintf(stderr, "ERROR: slave took too long, restart %d\n",
                pinfo->child);
        _report_progress(pinfo, path, new_len, LMS_PROGRESS_STATUS_KILLED);
        if (lms_restart_slave(pinfo, _slave_work) != 0)
            return -4;
        return 1;
    } else {
        if (reply < 0) {
            fprintf(stderr, "ERROR: pid=%d failed to parse \"%s\".\n",
                    getpid(), path);
            _report_progress(pinfo, path, new_len,
                             LMS_PROGRESS_STATUS_ERROR_PARSE);
            return (-reply) << 8;
        } else {
            _report_progress(pinfo, path, new_len,
                             LMS_PROGRESS_STATUS_PROCESSED);
            return reply;
        }
    }
}

static int _process_dir(struct pinfo *pinfo, int base, char *path, const char *name);

static int
_process_unknown(struct pinfo *pinfo, int base, char *path, const char *name)
{
    int new_len, reply, r;
    struct stat st;

    new_len = _strcat(base, path, name);
    if (new_len < 0)
        return -1;

    if (stat(path, &st) != 0) {
        perror("stat");
        return -2;
    }

    if (S_ISREG(st.st_mode))
        return _process_file(pinfo, base, path, name);
    else if (S_ISDIR(st.st_mode))
        return _process_dir(pinfo, base, path, name);
    else {
        fprintf(stderr,
                "INFO: %s is neither a directory nor a regular file.\n", path);
        return -3;
    }
}

static int
_process_dir(struct pinfo *pinfo, int base, char *path, const char *name)
{
    DIR *dir;
    struct dirent *de;
    lms_t *lms;
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
    lms = pinfo->lms;
    while ((de = readdir(dir)) != NULL && !lms->stop_processing) {
        if (de->d_name[0] == '.')
            continue;
        if (de->d_type == DT_REG) {
            if (_process_file(pinfo, new_len, path, de->d_name) < 0) {
                fprintf(stderr,
                        "ERROR: unrecoverable error parsing file, "
                        "exit \"%s\".\n", path);
                path[new_len - 1] = '\0';
                r = -4;
                goto end;
            }
        } else if (de->d_type == DT_DIR) {
            if (_process_dir(pinfo, new_len, path, de->d_name) < 0) {
                fprintf(stderr,
                        "ERROR: unrecoverable error parsing dir, "
                        "exit \"%s\".\n", path);
                path[new_len - 1] = '\0';
                r = -5;
                goto end;
            }
        } else if (de->d_type == DT_UNKNOWN) {
            if (_process_unknown(pinfo, new_len, path, de->d_name) < 0) {
                fprintf(stderr,
                        "ERROR: unrecoverable error parsing DT_UNKNOWN, "
                        "exit \"%s\".\n", path);
                path[new_len - 1] = '\0';
                r = -6;
                goto end;
            }
        }
    }

  end:
    closedir(dir);
    return r;
}

static int
_lms_process_check_valid(lms_t *lms, const char *path)
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

static int
_process_trigger(struct pinfo *pinfo, const char *top_path)
{
    char path[PATH_SIZE], *bname;
    lms_t *lms = pinfo->lms;
    int len;
    int r;

    if (realpath(top_path, path) == NULL) {
        perror("realpath");
        return -1;
    }

    /* search '/' backwards, split dirname and basename, note realpath usage */
    len = strlen(path);
    for (; len >= 0 && path[len] != '/'; len--);
    len++;
    bname = strdup(path + len);
    if (bname == NULL) {
        perror("strdup");
        return -2;
    }

    lms->is_processing = 1;
    lms->stop_processing = 0;
    r = _process_dir(pinfo, len, path, bname);
    lms->is_processing = 0;
    lms->stop_processing = 0;
    free(bname);

    return r;
}

/**
 * Process the given directory.
 *
 * This will add or update media found in the given directory or its children.
 *
 * @param lms previously allocated Light Media Scanner instance.
 * @param top_path top directory to scan.
 *
 * @return On success 0 is returned.
 */
int
lms_process(lms_t *lms, const char *top_path)
{
    struct pinfo pinfo;
    int r, len;

    r = _lms_process_check_valid(lms, top_path);
    if (r < 0)
        return r;

    pinfo.lms = lms;

    if (lms_create_pipes(&pinfo) != 0) {
        r = -1;
        goto end;
    }

    if (lms_create_slave(&pinfo, _slave_work) != 0) {
        r = -2;
        goto close_pipes;
    }

    r = _process_trigger(&pinfo, top_path);

    lms_finish_slave(&pinfo, _master_send_finish);
  close_pipes:
    lms_close_pipes(&pinfo);
  end:
    return r;
}

void
lms_stop_processing(lms_t *lms)
{
    if (!lms)
        return;
    if (!lms->is_processing)
        return;

    lms->stop_processing = 1;
}
