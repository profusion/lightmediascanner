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

/**
 * @brief
 *
 * pls playlist parser.
 *
 * This parser doesn't actually parse the whole file, instead it just checks
 * for the header [playlist], then search the beginning and ending of the file
 * (at most PLS_MAX_N_ENTRIES_BYTES_LOOKUP bytes) in order to find out
 * NumberOfEntries=XXX line. If there are too many bogus (ie: empty) lines or
 * this line is inside the data declaration, then it will fail the parse.
 * In theory this should not happen, so let's wait for bug reports.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE 600
#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PLS_MAX_N_ENTRIES_BYTES_LOOKUP 64

static int
_pls_find_header(int fd)
{
    const char header[] = "[playlist]";
    char buf[sizeof(header) - 1];
    ssize_t r;

    /* skip out white spaces */
    do {
        r = read(fd, buf, 1);
        if (r < 0) {
            perror("read");
            return -1;
        } else if (r == 0) {
            fprintf(stderr, "ERROR: premature end of file.\n");
            return -2;
        }

        if (!isspace(buf[0]))
            break;
    } while (1);

    if (buf[0] != header[0])
        return -3;

    /* try to read rest (from the second on) of the header */
    r = read(fd, buf + 1, sizeof(buf) - 1);
    if (r < 0) {
        perror("read");
        return -4;
    } else if (r != sizeof(buf) - 1) {
        fprintf(stderr, "ERROR: premature end of file: read %d of %d bytes.\n",
                r, sizeof(buf) - 1);
        return -5;
    }

    if (memcmp(buf + 1, header + 1, sizeof(buf) - 1) != 0) {
        fprintf(stderr, "ERROR: invalid pls header '%.*s'\n",
                sizeof(buf) - 1, buf);
        return -6;
    }

    /* find '\n' */
    do {
        r = read(fd, buf, 1);
        if (r < 0) {
            perror("read");
            return -7;
        } else if (r == 0)
            return -8;

        if (buf[0] == '\n')
            return 0;
    } while (1);

    return -1;
}

static int
_pls_find_n_entries_start(int fd, struct lms_playlist_info *info)
{
    char buf[PLS_MAX_N_ENTRIES_BYTES_LOOKUP];
    const char n_entries[] = "NumberOfEntries=";
    ssize_t r;
    int i;
    off_t off;

    off = lseek(fd, 0, SEEK_CUR);
    if (off < 0) {
        perror("lseek");
        return -1;
    }

    r = read(fd, buf, sizeof(buf));
    if (r < 0) {
        perror("read");
        return -2;
    } else if (r == 0)
        return -3;

    for (i = 0; i < r; i++) {
        char c;

        c = buf[i];
        if (c == n_entries[0]) {
            const char *p;
            i++;
            if (memcmp(buf + i, n_entries + 1, sizeof(n_entries) - 2) != 0) {
                off += i + sizeof(n_entries) - 2;
                goto done;
            }

            i += sizeof(n_entries) - 2;
            p = buf + i;
            for (; i < r; i++)
                if (buf[i] == '\n')
                    break;

            if (i == r) {
                fprintf(stderr, "WARNING: missing end of line\n");
                i = r - 1;
            }
            buf[i] = '\0';
            info->n_entries = atoi(p);
            return 0;
        } else if (c == 'V') {
            /* skip possible 'Version=XX' */
            for (i++; i < r; i++)
                if (buf[i] == '\n')
                    break;
        } else if (isspace(c))
            continue;
        else {
            off += i;
            goto done;
        }
    }

  done:
    /* not at the file beginning, reset offset */
    if (lseek(fd, off, SEEK_SET) < 0) {
        perror("lseek");
        return -1;
    }

    return 1;
}

static int
_pls_parse_entries_line(int fd, struct lms_playlist_info *info, char *buf, int len)
{
    const char n_entries[] = "NumberOfEntries=";
    int i;

    for (i = 0; i < len; i++, buf++)
        if (!isspace(*buf))
            break;

    if (i == len)
        return 1;
    len -= i;

    if (memcmp(buf, n_entries, sizeof(n_entries) - 1) != 0)
        return 1;

    buf += sizeof(n_entries) - 1;
    len -= sizeof(n_entries) - 1;
    buf[len] = '\0';

    info->n_entries = atoi(buf);
    return 0;
}

static int
_pls_find_n_entries_end(int fd, const struct lms_file_info *finfo, struct lms_playlist_info *info)
{
    char buf[PLS_MAX_N_ENTRIES_BYTES_LOOKUP];
    ssize_t r;
    int i, last_nl;

    if (finfo->size > sizeof(buf))
        if (lseek(fd, finfo->size - sizeof(buf), SEEK_SET) < 0) {
            perror("lseek");
            return -1;
        }

    r = read(fd, buf, sizeof(buf));
    if (r < 0) {
        perror("read");
        return -1;
    } else if (r == 0)
        return -2;

    last_nl = -1;
    for (i = r - 1; i >= 0; i--) {
        if (buf[i] == '\n') {
            if (last_nl >= 0) {
                int len;

                len = last_nl - i - 1;
                if (len > 0) {
                    int ret;

                    ret = _pls_parse_entries_line(fd, info, buf + i + 1, len);
                    if (ret <= 0)
                        return ret;
                }
            }
            last_nl = i;
        }
    }

    return 1;
}

static int
_pls_parse(int fd, const struct lms_file_info *finfo, struct lms_playlist_info *info)
{
    int r;

    r = _pls_find_header(fd);
    if (r != 0) {
        fprintf(stderr, "ERROR: could not find pls header. code=%d\n", r);
        return -1;
    }

    r = _pls_find_n_entries_start(fd, info);
    if (r <= 0)
        return r;

    r = _pls_find_n_entries_end(fd, finfo, info);
    if (r != 0)
        fprintf(stderr, "ERROR: could not find pls NumberOfEntries=\n");

    return r;
}

static const char _name[] = "pls";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".pls")
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_playlist_t *playlist_db;
};

static void *
_match(struct plugin *p, const char *path, int len, int base)
{
    int i;

    i = lms_which_extension(path, len, _exts, LMS_ARRAY_SIZE(_exts));
    if (i < 0)
      return NULL;
    else
      return (void*)(i + 1);
}

static int
_parse(struct plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    struct lms_playlist_info info = {0};
    int fd, r, ext_idx;

    fd = open(finfo->path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (_pls_parse(fd, finfo, &info) != 0) {
        fprintf(stderr,
                "WARNING: could not parse playlist '%s'.\n", finfo->path);
        return -1;
    }

    ext_idx = ((int)match) - 1;
    info.title.len = finfo->path_len - finfo->base - _exts[ext_idx].len;
    info.title.str = malloc((info.title.len + 1) * sizeof(char));
    memcpy(info.title.str, finfo->path + finfo->base, info.title.len);
    info.title.str[info.title.len] = '\0';

    info.id = finfo->id;
    r = lms_db_playlist_add(plugin->playlist_db, &info);

    if (info.title.str)
        free(info.title.str);
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    close(fd);

    return r;
}

static int
_setup(struct plugin *plugin, struct lms_context *ctxt)
{
    plugin->playlist_db = lms_db_playlist_new(ctxt->db);
    if (!plugin->playlist_db)
        return -1;

    return 0;
}

static int
_start(struct plugin *plugin, struct lms_context *ctxt)
{
    return lms_db_playlist_start(plugin->playlist_db);
}

static int
_finish(struct plugin *plugin, struct lms_context *ctxt)
{
    if (plugin->playlist_db)
        return lms_db_playlist_free(plugin->playlist_db);

    return 0;
}


static int
_close(struct plugin *plugin)
{
    free(plugin);
    return 0;
}

API struct lms_plugin *
lms_plugin_open(void)
{
    struct plugin *plugin;

    plugin = malloc(sizeof(*plugin));
    plugin->plugin.name = _name;
    plugin->plugin.match = (lms_plugin_match_fn_t)_match;
    plugin->plugin.parse = (lms_plugin_parse_fn_t)_parse;
    plugin->plugin.close = (lms_plugin_close_fn_t)_close;
    plugin->plugin.setup = (lms_plugin_setup_fn_t)_setup;
    plugin->plugin.start = (lms_plugin_start_fn_t)_start;
    plugin->plugin.finish = (lms_plugin_finish_fn_t)_finish;

    return (struct lms_plugin *)plugin;
}
