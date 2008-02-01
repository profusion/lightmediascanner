/**
 * Copyright (C) 2008 by INdT
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
 * @author Andre Moreira Magalhaes <andre.magalhaes@openbossa.org>
 */

/**
 * @brief
 *
 * real media file parser.
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BE_4BYTE(a) ((((unsigned char*)a)[0] << 24) |                   \
                     (((unsigned char*)a)[1] << 16) |                   \
                     (((unsigned char*)a)[2] << 8) |                    \
                     ((unsigned char*)a)[3])
#define BE_2BYTE(a) ((((unsigned char*)a)[0] << 8) | ((unsigned char*)a)[1])

enum StreamTypes {
    STREAM_TYPE_UNKNOWN = 0,
    STREAM_TYPE_AUDIO,
    STREAM_TYPE_VIDEO
};

struct rm_info {
    struct lms_string_size title;
    struct lms_string_size artist;
};

struct rm_file_header {
    char type[4];
    uint32_t size;
    uint16_t version;
} __attribute__((packed));

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
    lms_db_video_t *video_db;
};

static const char _name[] = "rm";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".ra"),
    LMS_STATIC_STRING_SIZE(".rv"),
    LMS_STATIC_STRING_SIZE(".rm"),
    LMS_STATIC_STRING_SIZE(".rmj"),
    LMS_STATIC_STRING_SIZE(".rmvb")
};

/*
 * A real media file header has the following format:
 * dword chunk type ('.RMF')
 * dword chunk size (typically 0x12)
 * word  chunk version
 * dword file version
 * dword number of headers
 */
static int
_parse_file_header(int fd, struct rm_file_header *file_header)
{
    if (read(fd, file_header, sizeof(struct rm_file_header)) == -1) {
        fprintf(stderr, "ERROR: could not read file header\n");
        return -1;
    }

    if (memcmp(file_header->type, ".RMF", 4) != 0) {
        fprintf(stderr, "ERROR: invalid header type\n");
        return -1;
    }

    /* convert to host byte order */
    file_header->size = BE_4BYTE(&file_header->size);

#if 0
    fprintf(stderr, "file_header type=%.*s\n", 4, file_header->type);
    fprintf(stderr, "file_header size=%d\n", file_header->size);
    fprintf(stderr, "file_header version=%d\n", file_header->version);
#endif

    /* TODO we should ignore these fields just when version is 0 or 1,
     * but using the test files, if we don't ignore them for version 256
     * it fails */
    /* ignore file header extra fields
     * file version and number of headers */
    lseek(fd, 8, SEEK_CUR);

    return 0;
}

static int
_read_header_type_and_size(int fd, char *type, uint32_t *size)
{
    if (read(fd, type, 4) != 4)
        return -1;

    if (read(fd, size, 4) != 4)
        return -1;

    *size = BE_4BYTE(size);

#if 0
    fprintf(stderr, "header type=%.*s\n", 4, type);
    fprintf(stderr, "header size=%d\n", *size);
#endif

    return 0;
}

static int
_read_string(int fd, char **out, unsigned int *out_len)
{
    char *s;
    uint16_t len;

    if (read(fd, &len, 2) == -1)
        return -1;

    len = BE_2BYTE(&len);

    if (out) {
        if (len > 0) {
            s = malloc(sizeof(char) * (len + 1));
            if (read(fd, s, len) == -1) {
                free(s);
                return -1;
            }
            s[len] = '\0';
            *out = s;
        } else
            *out = NULL;

        *out_len = len;
    } else
        lseek(fd, len, SEEK_CUR);

    return 0;
}

/*
 * A CONT header has the following format
 * dword   Chunk type ('CONT')
 * dword   Chunk size
 * word    Chunk version (always 0, for every known file)
 * word    Title string length
 * byte[]  Title string
 * word    Author string length
 * byte[]  Author string
 * word    Copyright string length
 * byte[]  Copyright string
 * word    Comment string length
 * byte[]  Comment string
 */
static void
_parse_cont_header(int fd, struct rm_info *info)
{
    /* Ps.: type and size were already read */

    /* ignore version */
    lseek(fd, 2, SEEK_CUR);

    _read_string(fd, &info->title.str, &info->title.len);
    _read_string(fd, &info->artist.str, &info->artist.len);
    _read_string(fd, NULL, NULL); /* copyright */
    _read_string(fd, NULL, NULL); /* comment */
}

static void
_strstrip(char **str, unsigned int *p_len)
{
    if (*str)
        lms_strstrip(*str, p_len);

    if (*p_len == 0 && *str) {
        free(*str);
        *str = NULL;
    }
}

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
    struct rm_info info = {{0}, {0}};
    struct lms_audio_info audio_info = {0, {0}, {0}, {0}, {0}, 0, 0, 0};
    struct lms_video_info video_info = {0, {0}, {0}};
    int r, fd, stream_type = STREAM_TYPE_UNKNOWN;
    struct rm_file_header file_header;
    char type[4];
    uint32_t size;

    fd = open(finfo->path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (_parse_file_header(fd, &file_header) != 0) {
        r = -2;
        goto done;
    }

    if (_read_header_type_and_size(fd, type, &size) != 0) {
        r = -3;
        goto done;
    }
    while (memcmp(type, "DATA", 4) != 0) {
        if (memcmp(type, "CONT", 4) == 0) {
            _parse_cont_header(fd, &info);
            break;
        }
        /* TODO check for mimetype
        else if (memcmp(type, "MDPR", 4) == 0) {
        }
        */
        /* ignore other headers */
        else
            lseek(fd, size - 8, SEEK_CUR);

        if (_read_header_type_and_size(fd, type, &size) != 0) {
            r = -4;
            goto done;
        }
    }

    /* try to define stream type by extension */
    if (stream_type == STREAM_TYPE_UNKNOWN) {
        int ext_idx = ((int)match) - 1;
        if (strcmp(_exts[ext_idx].str, ".ra") == 0)
            stream_type = STREAM_TYPE_AUDIO;
        /* consider rv, rm, rmj and rmvb as video */
        else
            stream_type = STREAM_TYPE_VIDEO;
    }

    _strstrip(&info.title.str, &info.title.len);
    _strstrip(&info.artist.str, &info.artist.len);

    if (!info.title.str) {
        int ext_idx;
        ext_idx = ((int)match) - 1;
        info.title.len = finfo->path_len - finfo->base - _exts[ext_idx].len;
        info.title.str = malloc((info.title.len + 1) * sizeof(char));
        memcpy(info.title.str, finfo->path + finfo->base, info.title.len);
        info.title.str[info.title.len] = '\0';
    }
    lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);

    if (info.artist.str)
        lms_charset_conv(ctxt->cs_conv, &info.artist.str, &info.artist.len);

#if 0
    fprintf(stderr, "file %s info\n", finfo->path);
    fprintf(stderr, "\ttitle=%s\n", info.title);
    fprintf(stderr, "\tartist=%s\n", info.artist);
#endif

    if (stream_type == STREAM_TYPE_AUDIO) {
        audio_info.id = finfo->id;
        audio_info.title = info.title;
        audio_info.artist = info.artist;
        r = lms_db_audio_add(plugin->audio_db, &audio_info);
    }
    else {
        video_info.id = finfo->id;
        video_info.title = info.title;
        video_info.artist = info.artist;
        r = lms_db_video_add(plugin->video_db, &video_info);
    }

  done:
    if (info.title.str)
        free(info.title.str);
    if (info.artist.str)
        free(info.artist.str);

    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    close(fd);

    return r;
}

static int
_setup(struct plugin *plugin, struct lms_context *ctxt)
{
    plugin->audio_db = lms_db_audio_new(ctxt->db);
    if (!plugin->audio_db)
        return -1;
    plugin->video_db = lms_db_video_new(ctxt->db);
    if (!plugin->video_db)
        return -1;

    return 0;
}

static int
_start(struct plugin *plugin, struct lms_context *ctxt)
{
    int r;
    r = lms_db_audio_start(plugin->audio_db);
    r |= lms_db_video_start(plugin->video_db);
    return r;
}

static int
_finish(struct plugin *plugin, struct lms_context *ctxt)
{
    if (plugin->audio_db)
        lms_db_audio_free(plugin->audio_db);
    if (plugin->video_db)
        lms_db_video_free(plugin->video_db);

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

    plugin = (struct plugin *)malloc(sizeof(*plugin));
    plugin->plugin.name = _name;
    plugin->plugin.match = (lms_plugin_match_fn_t)_match;
    plugin->plugin.parse = (lms_plugin_parse_fn_t)_parse;
    plugin->plugin.close = (lms_plugin_close_fn_t)_close;
    plugin->plugin.setup = (lms_plugin_setup_fn_t)_setup;
    plugin->plugin.start = (lms_plugin_start_fn_t)_start;
    plugin->plugin.finish = (lms_plugin_finish_fn_t)_finish;

    return (struct lms_plugin *)plugin;
}
