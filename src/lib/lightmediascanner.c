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

#define PATH_SIZE PATH_MAX
#define DEFAULT_SLAVE_TIMEOUT 1000

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
    int slave_timeout;
    unsigned int is_processing:1;
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
_slave_work(lms_t *lms, struct fds *fds)
{
    int r, len, base;
    char path[PATH_SIZE];

    while (((r = _slave_recv_path(fds, &len, &base, path)) == 0) && len > 0) {
        int i;

        for (i = 0; i < lms->n_parsers; i++) {
            lms_plugin_t *plugin;

            plugin = lms->parsers[i].plugin;
            plugin->parse(plugin, path, len, base);
        }
        _slave_send_reply(fds, 0);
    }

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
        reply = 1;
        fprintf(stderr, "ERROR: slave took too long, restart %d\n",
                pinfo->child);
        if (_restart_slave(pinfo) != 0)
            return -4;
    }

    return reply;
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
        return -2;
    }

    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return -3;
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
                r = -4;
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
lms_new(void)
{
    lms_t *lms;

    lms = calloc(1, sizeof(lms_t));
    if (!lms) {
        perror("calloc");
        return NULL;
    }

    lms->slave_timeout = DEFAULT_SLAVE_TIMEOUT;

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

    for (i = 0; i < lms->n_parsers; i++) {
        if (lms->parsers[i].plugin == handle) {
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
                    return -5;
                }

                return 0;
            }
        }
    }

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
    return lms->is_processing;
}

int
lms_get_slave_timeout(const lms_t *lms)
{
    return lms->slave_timeout;
}

void lms_set_slave_timeout(lms_t *lms, int ms)
{
    lms->slave_timeout = ms;
}

