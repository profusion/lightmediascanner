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
 * @author Gustavo Sverzut Barbieri <gustavo.barbieri@openbossa.org>
 */

/**
 * @brief
 *
 * id3 file parser.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>
#include <lightmediascanner_charset_conv.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define ID3V2_HEADER_SIZE       10
#define ID3V2_FOOTER_SIZE       10

enum ID3Encodings {
    ID3_ENCODING_LATIN1 = 0,
    ID3_ENCODING_UTF16,
    ID3_ENCODING_UTF16BE,
    ID3_ENCODING_UTF8,
    ID3_ENCODING_UTF16LE,
    ID3_ENCODING_LAST
};
#define ID3_NUM_ENCODINGS ID3_ENCODING_LAST


#include "id3v1_genres.c"

struct id3_info {
    struct lms_string_size title;
    struct lms_string_size artist;
    struct lms_string_size album;
    struct lms_string_size genre;
    unsigned char trackno;
    int cur_artist_priority;
};

struct id3v2_frame_header {
    char frame_id[4];
    unsigned int frame_size;
    int compression;
    int data_length_indicator;
};

struct id3v1_tag {
    char title[30];
    char artist[30];
    char album[30];
    char year[4];
    char comments[30];
    char genre;
} __attribute__((packed));

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
    lms_charset_conv_t *cs_convs[ID3_NUM_ENCODINGS];
};

static const char _name[] = "id3";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".mp3"),
    LMS_STATIC_STRING_SIZE(".aac")
};

static unsigned int
_to_uint(const char *data, int data_size)
{
    unsigned int sum = 0;
    unsigned int last, i;

    last = data_size > 4 ? 3 : data_size - 1;

    for (i = 0; i <= last; i++)
        sum |= ((unsigned char) data[i]) << ((last - i) * 8);

    return sum;
}

static inline int
_is_id3v2_second_synch_byte(unsigned char byte)
{
    if (byte == 0xff)
        return 0;
    if ((byte & 0xE0) == 0xE0)
        return 1;
    return 0;
}

static long
_find_id3v2(int fd)
{
    static const char pattern[3] = "ID3";
    char buffer[3];
    unsigned int prev_part_match, prev_part_match_sync = 0;
    long buffer_offset;

    if (read(fd, buffer, sizeof(buffer)) != sizeof(buffer))
        return -1;

    if (memcmp(buffer, pattern, sizeof(pattern)) == 0)
        return 0;

    /* This loop is the crux of the find method.  There are three cases that we
     * want to account for:
     * (1) The previously searched buffer contained a partial match of the
     * search pattern and we want to see if the next one starts with the
     * remainder of that pattern.
     *
     * (2) The search pattern is wholly contained within the current buffer.
     *
     * (3) The current buffer ends with a partial match of the pattern.  We will
     * note this for use in the next iteration, where we will check for the rest
     * of the pattern.
     */
    buffer_offset = 0;
    prev_part_match_sync = 0;
    prev_part_match = 0;
    while (1) {
        const char *p, *p_end;

        /* (1) previous partial match */
        if (prev_part_match_sync) {
            if (_is_id3v2_second_synch_byte(buffer[0]))
                return -1;
            prev_part_match_sync = 0;
        }

        if (prev_part_match) {
            const int size = sizeof(buffer) - prev_part_match;
            const char *part_pattern = pattern + prev_part_match;

            if (memcmp(buffer, part_pattern, size) == 0)
                return buffer_offset - prev_part_match;

            prev_part_match = 0;
        }

        p_end = buffer + sizeof(buffer);
        for (p = buffer; p < p_end; p++) {
            if (*p == pattern[0]) {
                /* Try to match pattern, possible partial contents */
                const char *q;
                int todo;

                q = p + 1;
                todo = p_end - q;
                if (todo == 0 || memcmp(q, pattern + 1, todo) == 0) {
                    todo++;
                    if (todo == sizeof(buffer))
                        /* (2) pattern contained in current buffer */
                        return buffer_offset;

                    /* (3) partial match */
                    prev_part_match = todo;
                    break;
                }
            } else if ((unsigned char)*p == 0xff) {
                /* Try to match synch pattern, possible partial contents */
                const char *q;

                q = p + 1;
                if (q < p_end) {
                    if (_is_id3v2_second_synch_byte(*q))
                        /* (2) synch pattern contained in current buffer */
                        return -1;
                } else
                    /* (3) partial match */
                    prev_part_match_sync = 1;
            }
        }

        if (read(fd, buffer, sizeof(buffer)) != sizeof(buffer))
            return -1;
        buffer_offset += sizeof(buffer);
    }

    return -1;
}

static unsigned int
_get_id3v2_frame_header_size(unsigned int version)
{
    switch (version) {
    case 0:
    case 1:
    case 2:
        return 6;
    case 3:
    case 4:
    default:
        return 10;
    }
}

static void
_parse_id3v2_frame_header(char *data, unsigned int version, struct id3v2_frame_header *fh)
{
    switch (version) {
    case 0:
    case 1:
    case 2:
        memcpy(fh->frame_id, data, 3);
        fh->frame_id[3] = 0;
        fh->frame_size = _to_uint(data + 3, 3);
        fh->compression = 0;
        fh->data_length_indicator = 0;
        break;
    case 3:
        memcpy(fh->frame_id, data, 4);
        fh->frame_size = _to_uint(data + 4, 4);
        fh->compression = data[9] & 0x40;
        fh->data_length_indicator = 0;
        break;
    case 4:
    default:
        memcpy(fh->frame_id, data, 4);
        fh->frame_size = _to_uint(data + 4, 4);
        fh->compression = data[9] & 0x4;
        fh->data_length_indicator = data[9] & 0x1;
        break;
    }
}

static inline void
_get_id3v2_frame_info(const char *frame_data, unsigned int frame_size, struct lms_string_size *s, lms_charset_conv_t *cs_conv, int strip)
{
    if (frame_size > s->len)
        s->str = realloc(s->str, sizeof(char) * (frame_size + 1));
    memcpy(s->str, frame_data, frame_size);
    s->str[frame_size] = '\0';
    s->len = frame_size;
    if (cs_conv)
        lms_charset_conv(cs_conv, &s->str, &s->len);
    if (strip)
        lms_string_size_strip_and_free(s);
}

static int
_get_id3v2_artist(unsigned int index, const char *frame_data, unsigned int frame_size, struct id3_info *info, lms_charset_conv_t *cs_conv)
{
    static const unsigned char artist_priorities[] = {3, 4, 2, 1};
    const unsigned int index_max = sizeof(artist_priorities) / sizeof(*artist_priorities);

    if (index >= index_max)
        return 1;

    if (artist_priorities[index] > info->cur_artist_priority) {
        struct lms_string_size artist = {0};

        _get_id3v2_frame_info(frame_data, frame_size, &artist, cs_conv, 1);
        if (artist.str) {
            if (info->artist.str)
                free(info->artist.str);
            info->artist = artist;
            info->cur_artist_priority = artist_priorities[index];
        }
    }
    return 0;
}

static int
_get_id3v1_genre(unsigned int genre, struct lms_string_size *out)
{
    if (genre < ID3V1_NUM_GENRES) {
        unsigned int size, base, len;

        base = id3v1_genres_offsets[genre];
        size = id3v1_genres_offsets[genre + 1] - base;
        len = size - 1;

        if (len > out->len) {
            char *p = realloc(out->str, size);
            if (!p)
                return -2;
            out->str = p;
        }

        out->len = len;
        memcpy(out->str, id3v1_genres_mem + base, size);

        return 0;
    }
    return -1;
}

static inline int
_parse_id3v1_genre(const char *str_genre, struct lms_string_size *out)
{
    return _get_id3v1_genre(atoi(str_genre), out);
}

static void
_get_id3v2_genre(const char *frame_data, unsigned int frame_size, struct lms_string_size *out, lms_charset_conv_t *cs_conv)
{
    int i, is_number;
    struct lms_string_size genre = {0};

    _get_id3v2_frame_info(frame_data, frame_size, &genre, cs_conv, 1);

    if (!genre.str)
        return;

    is_number = (genre.len != 0 && genre.str[0] != '(');
    if (is_number) {
        for (i = 0; i < genre.len; ++i) {
            if (!isdigit(genre.str[i])) {
                is_number = 0;
                break;
            }
        }
    }

    if (is_number && _parse_id3v1_genre(genre.str, out) == 0) {
        /* id3v1 genre found */
        free(genre.str);
        return;
    }

    /* ID3v2.3 "content type" can contain a ID3v1 genre number in parenthesis at
     * the beginning of the field. If this is all that the field contains, do a
     * translation from that number to the name and return that.  If there is a
     * string folloing the ID3v1 genre number, that is considered to be
     * authoritative and we return that instead. Or finally, the field may
     * simply be free text, in which case we just return the value. */

    if (genre.len > 1 && genre.str[0] == '(') {
        char *closing = NULL;

        if (genre.str[genre.len - 1] == ')') {
            closing = strchr(genre.str, ')');
            if (closing == genre.str + genre.len - 1) {
                /* ) is the last character and only appears once in the
                 * string get the id3v1 genre enclosed by parentheses
                 */
                if (_parse_id3v1_genre(genre.str + 1, out) == 0) {
                    free(genre.str);
                    return;
                }
            }
        }

        /* get the string followed by the id3v1 genre */
        if (!closing)
            closing = strchr(genre.str, ')');

        if (closing) {
            closing++;
            out->len = genre.len - (closing - genre.str);
            out->str = genre.str;
            memmove(out->str, closing, out->len + 1); /* includes '\0' */
            lms_string_size_strip_and_free(out);
            return;
        }
    }

    /* pure text */
    *out = genre;
}

static void
_get_id3v2_trackno(const char *frame_data, unsigned int frame_size, struct id3_info *info, lms_charset_conv_t *cs_conv)
{
    struct lms_string_size trackno = {0};

    _get_id3v2_frame_info(frame_data, frame_size, &trackno, cs_conv, 0);
    info->trackno = atoi(trackno.str);
    free(trackno.str);
}

static void
_parse_id3v2_frame(struct id3v2_frame_header *fh, const char *frame_data, struct id3_info *info, lms_charset_conv_t **cs_convs)
{
    lms_charset_conv_t *cs_conv;
    unsigned int text_encoding, frame_size;
    const char *fid;

#if 0
    fprintf(stderr, "frame id = %.4s frame size = %d text encoding = %d\n",
            fh->frame_id, fh->frame_size, frame_data[0]);
#endif

    /* All used frames start with 'T' */
    fid = fh->frame_id;
    if (fid[0] != 'T')
        return;

    /* Latin1  = 0
     * UTF16   = 1
     * UTF16BE = 2
     * UTF8    = 3
     * UTF16LE = 4
     */
    text_encoding = frame_data[0];

    /* skip first byte - text encoding */
    frame_data += 1;
    frame_size = fh->frame_size - 1;

    if (text_encoding < ID3_NUM_ENCODINGS) {
        if (text_encoding == ID3_ENCODING_UTF16) {
            if (memcmp(frame_data, "\xfe\xff", 2) == 0)
                text_encoding = ID3_ENCODING_UTF16BE;
            else
                text_encoding = ID3_ENCODING_UTF16LE;
            frame_data += 2;
            frame_size -= 2;
        }
        cs_conv = cs_convs[text_encoding];
    } else
        cs_conv = NULL;

    /* ID3v2.2 used 3 bytes for the frame id, so let's check it */
    if ((fid[1] == 'T' && fid[2] == '2') ||
        (fid[1] == 'I' && fid[2] == 'T' && fid[3] == '2'))
        _get_id3v2_frame_info(frame_data, frame_size, &info->title, cs_conv, 1);
    else if (fid[1] == 'P') {
        if (fid[2] == 'E')
            _get_id3v2_artist(fid[3] - '1', frame_data, frame_size,
                              info, cs_conv);
        else if (fid[2] >= '1' && fid[2] <= '4')
            _get_id3v2_artist(fid[2] - '1', frame_data, frame_size,
                              info, cs_conv);
    }
    /* TALB, TAL */
    else if (fid[1] == 'A' && fid[2] == 'L')
        _get_id3v2_frame_info(frame_data, frame_size, &info->album, cs_conv, 1);
    /* TCON, TCO */
    else if (fid[1] == 'C' && fid[2] == 'O')
        _get_id3v2_genre(frame_data, frame_size, &info->genre, cs_conv);
    else if (fid[1] == 'R' && (fid[2] == 'K' ||
                               (fid[2] == 'C' && fid[3] == 'K')))
        _get_id3v2_trackno(frame_data, frame_size, info, cs_conv);
}

static int
_parse_id3v2(int fd, long id3v2_offset, struct id3_info *info, lms_charset_conv_t **cs_convs)
{
    char header_data[10], frame_header_data[10];
    unsigned int tag_size, major_version, frame_data_pos, frame_data_length, frame_header_size;
    int extended_header, footer_present;
    struct id3v2_frame_header fh;
    size_t nread;

    lseek(fd, id3v2_offset, SEEK_SET);

    /* parse header */
    if (read(fd, header_data, ID3V2_HEADER_SIZE) != ID3V2_HEADER_SIZE)
        return -1;

    tag_size = _to_uint(header_data + 6, 4);
    if (tag_size == 0)
        return -1;

    /* parse frames */
    major_version = header_data[3];

    frame_data_pos = 0;
    frame_data_length = tag_size;

    /* check for extended header */
    extended_header = header_data[5] & 0x20; /* bit 6 */
    if (extended_header) {
        /* skip extended header */
        unsigned int extended_header_size;
        char extended_header_data[4];

        if (read(fd, extended_header_data, 4) != 4)
            return -1;
        extended_header_size = _to_uint(extended_header_data, 4);
        lseek(fd, extended_header_size - 4, SEEK_CUR);
        frame_data_pos += extended_header_size;
        frame_data_length -= extended_header_size;
    }

    footer_present = header_data[5] & 0x8;   /* bit 4 */
    if (footer_present && frame_data_length > ID3V2_FOOTER_SIZE)
        frame_data_length -= ID3V2_FOOTER_SIZE;

    frame_header_size = _get_id3v2_frame_header_size(major_version);
    while (frame_data_pos < frame_data_length - frame_header_size) {
        nread = read(fd, frame_header_data, frame_header_size);
        if (nread == 0)
            break;

        if (nread != frame_header_size)
            return -1;

        if (frame_header_data[0] == 0)
            break;

        _parse_id3v2_frame_header(frame_header_data, major_version, &fh);
        if (!fh.frame_size)
            break;

        if (!fh.compression &&
            fh.frame_id[0] == 'T' &&
            memcmp(fh.frame_id, "TXXX", 4) != 0) {
            char *frame_data;

            if (fh.data_length_indicator)
                lseek(fd, 4, SEEK_CUR);

            frame_data = malloc(sizeof(char) * fh.frame_size);
            if (read(fd, frame_data, fh.frame_size) != fh.frame_size) {
                free(frame_data);
                return -1;
            }

            _parse_id3v2_frame(&fh, frame_data, info, cs_convs);
            free(frame_data);
        }
        else {
            if (fh.data_length_indicator)
                lseek(fd, fh.frame_size + 4, SEEK_CUR);
            else
                lseek(fd, fh.frame_size, SEEK_CUR);
        }

        frame_data_pos += fh.frame_size + frame_header_size;
    }

    return 0;
}

static int
_parse_id3v1(int fd, struct id3_info *info, lms_charset_conv_t *cs_conv)
{
    struct id3v1_tag tag;
    if (read(fd, &tag, sizeof(struct id3v1_tag)) == -1)
        return -1;

    info->title.str = strndup(tag.title, 30);
    info->title.len = strlen(info->title.str);
    lms_charset_conv(cs_conv, &info->title.str, &info->title.len);
    info->artist.str = strndup(tag.artist, 30);
    info->artist.len = strlen(info->artist.str);
    lms_charset_conv(cs_conv, &info->artist.str, &info->artist.len);
    info->album.str = strndup(tag.album, 30);
    info->album.len = strlen(info->album.str);
    lms_charset_conv(cs_conv, &info->album.str, &info->album.len);
    _get_id3v1_genre(tag.genre, &info->genre);
    if (tag.comments[28] == 0 && tag.comments[29] != 0)
        info->trackno = (unsigned char) tag.comments[29];

    return 0;
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
    struct id3_info info = {{0}, {0}, {0}, {0}, 0, -1};
    struct lms_audio_info audio_info = {0, {0}, {0}, {0}, {0}, 0, 0, 0};
    int r, fd;
    long id3v2_offset;

    fd = open(finfo->path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    id3v2_offset = _find_id3v2(fd);
    if (id3v2_offset >= 0) {
#if 0
        fprintf(stderr, "id3v2 tag found in file %s with offset %ld\n",
                finfo->path, id3v2_offset);
#endif
        if (_parse_id3v2(fd, id3v2_offset, &info, plugin->cs_convs) != 0) {
            r = -2;
            goto done;
        }
    }
    else {
        char tag[3];
#if 0
        fprintf(stderr, "id3v2 tag not found in file %s. trying id3v1\n",
                finfo->path);
#endif
        /* check for id3v1 tag */
        if (lseek(fd, -128, SEEK_END) == -1) {
            r = -3;
            goto done;
        }

        if (read(fd, &tag, 3) == -1) {
            r = -4;
            goto done;
        }

        if (memcmp(tag, "TAG", 3) == 0) {
#if 0
            fprintf(stderr, "id3v1 tag found in file %s\n", finfo->path);
#endif
            if (_parse_id3v1(fd, &info, ctxt->cs_conv) != 0) {
                r = -5;
                goto done;
            }
        }
    }

    if (!info.title.str) {
        int ext_idx;
        ext_idx = ((int)match) - 1;
        info.title.len = finfo->path_len - finfo->base - _exts[ext_idx].len;
        info.title.str = malloc((info.title.len + 1) * sizeof(char));
        memcpy(info.title.str, finfo->path + finfo->base, info.title.len);
        info.title.str[info.title.len] = '\0';
        lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);
    }

#if 0
    fprintf(stderr, "file %s info\n", finfo->path);
    fprintf(stderr, "\ttitle='%s'\n", info.title.str);
    fprintf(stderr, "\tartist='%s'\n", info.artist.str);
    fprintf(stderr, "\talbum='%s'\n", info.album.str);
    fprintf(stderr, "\tgenre='%s'\n", info.genre.str);
    fprintf(stderr, "\ttrack number='%d'\n", info.trackno);
#endif

    audio_info.id = finfo->id;
    audio_info.title = info.title;
    audio_info.artist = info.artist;
    audio_info.album = info.album;
    audio_info.genre = info.genre;
    audio_info.trackno = info.trackno;
    r = lms_db_audio_add(plugin->audio_db, &audio_info);

  done:
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    close(fd);

    if (info.title.str)
        free(info.title.str);
    if (info.artist.str)
        free(info.artist.str);
    if (info.album.str)
        free(info.album.str);
    if (info.genre.str)
        free(info.genre.str);

    return r;
}

static int
_setup(struct plugin *plugin, struct lms_context *ctxt)
{
    int i;
    const char *id3_encodings[ID3_NUM_ENCODINGS] = {
        "Latin1",
        NULL, /* UTF-16 */
        "UTF-16BE",
        NULL, /* UTF-8 */
        "UTF-16LE",
    };

    plugin->audio_db = lms_db_audio_new(ctxt->db);
    if (!plugin->audio_db)
        return -1;

    for (i = 0; i < ID3_NUM_ENCODINGS; ++i) {
        /* do not create charset conv for UTF-8 encoding */
        if (!id3_encodings[i]) {
            plugin->cs_convs[i] = NULL;
            continue;
        }
        plugin->cs_convs[i] = lms_charset_conv_new_full(0, 0);
        if (!plugin->cs_convs[i])
            return -1;
        lms_charset_conv_add(plugin->cs_convs[i], id3_encodings[i]);
    }

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
    int i;

    if (plugin->audio_db)
        lms_db_audio_free(plugin->audio_db);

    for (i = 0; i < ID3_NUM_ENCODINGS; ++i) {
        if (plugin->cs_convs[i])
            lms_charset_conv_free(plugin->cs_convs[i]);
    }

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
