/**
 * Copyright (C) 2013  Intel Corporation. All rights reserved.
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
 * @author Lucas De Marchi <lucas.demarchi@intel.com>
 */

/**
 * @brief
 *
 * wave file parser.
 *
 * References:
 *   Format specification:
 *     http://www-mmsp.ece.mcgill.ca/documents/AudioFormats/WAVE/WAVE.html
 *     http://www.sonicspot.com/guide/wavefiles.html
 *   INFO tags:
 *     http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/RIFF.html#Info
 *     http://www.aelius.com/njh/wavemetatools/bsiwave_tag_map.html
 */

#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>
#include <lightmediascanner_charset_conv.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <shared/util.h>

static const char _name[] = "wave";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".wav"),
    LMS_STATIC_STRING_SIZE(".wave"),
};
static const char *_cats[] = {
    "audio",
    NULL
};
static const char *_authors[] = {
    "Lucas De Marchi",
    NULL
};
static const struct lms_string_size _container = LMS_STATIC_STRING_SIZE("wave");

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
};

static void *
_match(struct plugin *p, const char *path, int len, int base)
{
    long i;

    i = lms_which_extension(path, len, _exts, LMS_ARRAY_SIZE(_exts));
    if (i < 0)
        return NULL;
    else
        return (void*)(i + 1);
}

static int
_parse_info(int fd, struct lms_audio_info *info)
{
    struct hdr {
        uint8_t id[4];
        uint32_t size;
        uint8_t infoid[4];
    } __attribute__((packed)) hdr;
    long maxsize = 0;

    /* Search the LIST INFO chunk */
    do {
        uint32_t size;

        if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
            goto done;

        size = get_le32(&hdr.size) - sizeof(hdr.infoid);

        if (memcmp(hdr.id, "LIST", 4) == 0
            && memcmp(hdr.infoid, "INFO", 4) == 0) {
            maxsize = size;
            break;
        } else if ((size & 0x1) && memcmp(hdr.id, "data", 4) == 0)
            /* 'data' chunk has a 1-byte padding if size is odd */
            size++;

        lseek(fd, size, SEEK_CUR);
    } while (1);

    while (maxsize > 8) {
        uint8_t chunkid[4];
        uint32_t size;
        struct lms_string_size *str;

        if (read(fd, chunkid, sizeof(chunkid)) != sizeof(chunkid)
            || read(fd, &size, sizeof(size)) != sizeof(size))
            break;

        size = le32toh(size);

        if (memcmp(chunkid, "INAM", 4) == 0)
            str = &info->title;
        else if (memcmp(chunkid, "IART", 4) == 0)
            str = &info->artist;
        else if (memcmp(chunkid, "IPRD", 4) == 0)
            str = &info->album;
        else if (memcmp(chunkid, "IGNR", 4) == 0)
            str = &info->genre;
        else {
            lseek(fd, size, SEEK_CUR);
            maxsize -= size;
            goto next_field;
        }

        str->str = malloc(size + 1);
        read(fd, str->str, size);
        str->str[size] = '\0';
        str->len = size;

    next_field:
        /* Ignore trailing '\0', even if they are not part of the previous
         * size */
        while (maxsize > 0 && read(fd, chunkid, 1) == 1) {
            if (chunkid[0] != '\0') {
                lseek(fd, -1, SEEK_CUR);
                break;
            }
            maxsize--;
        }
    }

done:
    return 0;
}

static int
_parse_fmt(int fd, struct lms_audio_info *info)
{
    struct fmt {
        uint8_t fmt[4];
        uint32_t size;
        uint16_t audio_format;
        uint16_t n_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bps;
    } __attribute__((packed)) fmt;
    long remain;

    if (read(fd, &fmt, sizeof(fmt)) != sizeof(fmt)
        || memcmp(fmt.fmt, "fmt ", 4) != 0)
        return -1;

    info->channels = get_le16(&fmt.n_channels);
    info->bitrate = get_le32(&fmt.byte_rate) * 8;
    info->sampling_rate = get_le32(&fmt.sample_rate);

    remain = get_le32(&fmt.size) - sizeof(fmt) +
        offsetof(struct fmt, audio_format);
    if (remain > 0)
        lseek(fd, remain, SEEK_CUR);

    return 0;
}

static int
_parse_wave(int fd, struct lms_audio_info *info)
{
    struct hdr {
        uint8_t riff[4];
        uint32_t size;
        uint8_t wave[4];
    } __attribute__((packed)) hdr;

    if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)
        || memcmp(hdr.riff, "RIFF", 4) != 0
        || memcmp(hdr.wave, "WAVE", 4) != 0
        || _parse_fmt(fd, info) < 0)
        return -1;

    info->container = _container;

    return 0;
}

static int
_parse(struct plugin *plugin, struct lms_context *ctxt,
       const struct lms_file_info *finfo, void *match)
{
    struct lms_audio_info info = { };
    int r, fd;

    fd = open(finfo->path, O_RDONLY);
    if (fd < 0)
        return -errno;

    r = _parse_wave(fd, &info);
    if (r < 0)
        goto done;

    /* Ignore errors, waves likely don't have any additional information */
    _parse_info(fd, &info);

    if (!info.title.str) {
        long ext_idx = ((long)match) - 1;
        info.title.len = finfo->path_len - finfo->base - _exts[ext_idx].len;
        info.title.str = malloc((info.title.len + 1) * sizeof(char));
        memcpy(info.title.str, finfo->path + finfo->base, info.title.len);
        info.title.str[info.title.len] = '\0';
        lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);
    }

    info.id = finfo->id;
    r = lms_db_audio_add(plugin->audio_db, &info);

done:
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    close(fd);

    free(info.title.str);
    free(info.artist.str);
    free(info.album.str);
    free(info.genre.str);

    return r;
}

static int
_setup(struct plugin *plugin,  struct lms_context *ctxt)
{
    plugin->audio_db = lms_db_audio_new(ctxt->db);
    if (!plugin->audio_db)
        return -1;

    return 0;
}

static int
_start(struct plugin *plugin, struct lms_context *ctxt)
{

    return lms_db_audio_start(plugin->audio_db);
}

static int
_finish(struct plugin *plugin, struct lms_context *ctxt)
{
    if (plugin->audio_db)
        lms_db_audio_free(plugin->audio_db);

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
    struct lms_plugin *plugin;

    plugin = (struct lms_plugin *) malloc(sizeof(struct plugin));
    plugin->name = _name;
    plugin->match = (lms_plugin_match_fn_t) _match;
    plugin->parse = (lms_plugin_parse_fn_t) _parse;
    plugin->close = (lms_plugin_close_fn_t) _close;
    plugin->setup = (lms_plugin_setup_fn_t) _setup;
    plugin->start = (lms_plugin_start_fn_t) _start;
    plugin->finish = (lms_plugin_finish_fn_t) _finish;

    return plugin;
}

API const struct lms_plugin_info *
lms_plugin_info(void)
{
    static struct lms_plugin_info info = {
        _name,
        _cats,
	"Wave files",
        PACKAGE_VERSION,
        _authors,
        "http://01.org"
    };

    return &info;
}
