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
 * id3 file parser.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define ID3V2_GET_FRAME_INFO(frame_data, frame_size, str, len) { \
    str = malloc(sizeof(char) * (frame_size + 1)); \
    memcpy(str, frame_data, frame_size); \
    str[frame_size] = '\0'; \
    len = frame_size; \
}

#define ID3V2_HEADER_SIZE       10
#define ID3V2_FOOTER_SIZE       10
#define ID3V2_FRAME_HEADER_SIZE 10

#define ID3V1_NUM_GENRES 148

static const char *id3v1_genres[ID3V1_NUM_GENRES] = {
    "Blues",
    "Classic Rock",
    "Country",
    "Dance",
    "Disco",
    "Funk",
    "Grunge",
    "Hip-Hop",
    "Jazz",
    "Metal",
    "New Age",
    "Oldies",
    "Other",
    "Pop",
    "R&B",
    "Rap",
    "Reggae",
    "Rock",
    "Techno",
    "Industrial",
    "Alternative",
    "Ska",
    "Death Metal",
    "Pranks",
    "Soundtrack",
    "Euro-Techno",
    "Ambient",
    "Trip-Hop",
    "Vocal",
    "Jazz+Funk",
    "Fusion",
    "Trance",
    "Classical",
    "Instrumental",
    "Acid",
    "House",
    "Game",
    "Sound Clip",
    "Gospel",
    "Noise",
    "Alternative Rock",
    "Bass",
    "Soul",
    "Punk",
    "Space",
    "Meditative",
    "Instrumental Pop",
    "Instrumental Rock",
    "Ethnic",
    "Gothic",
    "Darkwave",
    "Techno-Industrial",
    "Electronic",
    "Pop-Folk",
    "Eurodance",
    "Dream",
    "Southern Rock",
    "Comedy",
    "Cult",
    "Gangsta",
    "Top 40",
    "Christian Rap",
    "Pop/Funk",
    "Jungle",
    "Native American",
    "Cabaret",
    "New Wave",
    "Psychedelic",
    "Rave",
    "Showtunes",
    "Trailer",
    "Lo-Fi",
    "Tribal",
    "Acid Punk",
    "Acid Jazz",
    "Polka",
    "Retro",
    "Musical",
    "Rock & Roll",
    "Hard Rock",
    "Folk",
    "Folk/Rock",
    "National Folk",
    "Swing",
    "Fusion",
    "Bebob",
    "Latin",
    "Revival",
    "Celtic",
    "Bluegrass",
    "Avantgarde",
    "Gothic Rock",
    "Progressive Rock",
    "Psychedelic Rock",
    "Symphonic Rock",
    "Slow Rock",
    "Big Band",
    "Chorus",
    "Easy Listening",
    "Acoustic",
    "Humour",
    "Speech",
    "Chanson",
    "Opera",
    "Chamber Music",
    "Sonata",
    "Symphony",
    "Booty Bass",
    "Primus",
    "Porn Groove",
    "Satire",
    "Slow Jam",
    "Club",
    "Tango",
    "Samba",
    "Folklore",
    "Ballad",
    "Power Ballad",
    "Rhythmic Soul",
    "Freestyle",
    "Duet",
    "Punk Rock",
    "Drum Solo",
    "A Cappella",
    "Euro-House",
    "Dance Hall",
    "Goa",
    "Drum & Bass",
    "Club-House",
    "Hardcore",
    "Terror",
    "Indie",
    "BritPop",
    "Negerpunk",
    "Polsk Punk",
    "Beat",
    "Christian Gangsta Rap",
    "Heavy Metal",
    "Black Metal",
    "Crossover",
    "Contemporary Christian",
    "Christian Rock",
    "Merengue",
    "Salsa",
    "Thrash Metal",
    "Anime",
    "Jpop",
    "Synthpop"
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
};

static const char _name[] = "id3";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".mp3"),
    LMS_STATIC_STRING_SIZE(".aac")
};

static int
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
    long buffer_offset = 0;
    char buffer[3], *p;
    int buffer_size = sizeof(buffer);
    const char pattern[] = "ID3";
    ssize_t nread;

    /* These variables are used to keep track of a partial match that happens at
     * the end of a buffer. */
    int previous_partial_match = -1;
    int previous_partial_synch_match = 0;
    int first_synch_byte;

    /* Start the search at the beginning of the file. */
    lseek(fd, 0, SEEK_SET);

    if ((nread = read(fd, &buffer, buffer_size)) != buffer_size)
        return -1;

    /* check if pattern is in the beggining of the file */
    if (memcmp(buffer, pattern, 3) == 0)
        return 0;

    /* This loop is the crux of the find method.  There are three cases that we
     * want to account for:
     * (1) The previously searched buffer contained a partial match of the search
     * pattern and we want to see if the next one starts with the remainder of
     * that pattern.
     *
     * (2) The search pattern is wholly contained within the current buffer.
     *
     * (3) The current buffer ends with a partial match of the pattern.  We will
     * note this for use in the next iteration, where we will check for the rest
     * of the pattern. */
    while (1) {
        /* (1) previous partial match */
        if (previous_partial_synch_match && _is_id3v2_second_synch_byte(buffer[0]))
            return -1;

        if (previous_partial_match >= 0 && previous_partial_match < buffer_size) {
            const int pattern_offset = buffer_size - previous_partial_match;

            if (memcmp(buffer, pattern + pattern_offset, 3 - pattern_offset) == 0)
                return buffer_offset - buffer_size + previous_partial_match;
        }

        /* (2) pattern contained in current buffer */
        p = buffer;
        while ((p = memchr(p, 'I', buffer_size))) {
            if (memcmp(p, pattern, 3) == 0)
                return buffer_offset + (p - buffer);
            p += 1;
        }

        p = memchr(buffer, 255, buffer_size);
        if (p)
            first_synch_byte = p - buffer;
        else
            first_synch_byte = -1;

        /* Here we have to loop because there could be several of the first
         * (11111111) byte, and we want to check all such instances until we find
         * a full match (11111111 111) or hit the end of the buffer. */
        while (first_synch_byte >= 0) {
            /* if this *is not* at the end of the buffer */
            if (first_synch_byte < buffer_size - 1) {
                if(_is_id3v2_second_synch_byte(buffer[first_synch_byte + 1]))
                    /* We've found the frame synch pattern. */
                    return -1;
                else
                    /* We found 11111111 at the end of the current buffer indicating a
                     * partial match of the synch pattern.  The find() below should
                     * return -1 and break out of the loop. */
                    previous_partial_synch_match = 1;
            }

            /* Check in the rest of the buffer. */
            p = memchr(p + 1, 255, buffer_size);
            if (p)
                first_synch_byte = p - buffer;
            else
                first_synch_byte = -1;
        }

        /* (3) partial match */
        if (buffer[nread - 1] == pattern[1])
            previous_partial_match = nread - 1;
        else if (memcmp(&buffer[nread - 2], pattern, 2) == 0)
            previous_partial_match = nread - 2;
        buffer_offset += buffer_size;

        if ((nread = read(fd, &buffer, sizeof(buffer))) == -1)
            return -1;
    }

    return -1;
}

static unsigned int
_to_uint(const char *data, int data_size)
{
    unsigned int sum = 0;
    int last = data_size > 4 ? 3 : data_size - 1;
    int i;

    for (i = 0; i <= last; i++)
        sum |= (data[i] & 0x7f) << ((last - i) * 7);

    return sum;
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

static void
_parse_id3v2_frame(struct id3v2_frame_header *fh, const char *frame_data, struct lms_audio_info *info)
{
    unsigned int frame_size;

    /* TODO proper handle text encoding
     *
     * Latin1  = 0
     * UTF16   = 1
     * UTF16BE = 2
     * UTF8    = 3
     * UTF16LE = 4
     */

    /* skip first byte - text encoding */
    frame_data += 1;
    frame_size = fh->frame_size - 1;

    if (memcmp(fh->frame_id, "TIT2", 4) == 0)
        ID3V2_GET_FRAME_INFO(frame_data, frame_size, info->title.str, info->title.len)
    else if (memcmp(fh->frame_id, "TPE1", 4) == 0)
        ID3V2_GET_FRAME_INFO(frame_data, frame_size, info->artist.str, info->artist.len)
    else if (memcmp(fh->frame_id, "TALB", 4) == 0)
        ID3V2_GET_FRAME_INFO(frame_data, frame_size, info->album.str, info->album.len)
    else if (memcmp(fh->frame_id, "TCON", 4) == 0) {
        /* TODO handle multiple genres */
        int i, is_number;

        ID3V2_GET_FRAME_INFO(frame_data, frame_size, info->genre.str, info->genre.len)

        is_number = 1;
        for (i = 0; i < info->genre.len; ++i) {
            if (!isdigit(info->genre.str[i]))
                is_number = 0;
        }

        if ((is_number) &&
            (info->genre.str) &&
            (info->genre.len > 0)) {
            int genre = atoi(info->genre.str);
            if (genre >= 0 && genre < ID3V1_NUM_GENRES) {
                free(info->genre.str);
                info->genre.str = strdup(id3v1_genres[(int) genre]);
                info->genre.len = strlen(info->genre.str);
            }
            else {
                /* ignore other genres */
                free(info->genre.str);
                info->genre.str = NULL;
                info->genre.len = 0;
            }
        }
    }
    else if (memcmp(fh->frame_id, "TRCK", 4) == 0) {
        char *trackno;
        unsigned int trackno_len;
        ID3V2_GET_FRAME_INFO(frame_data, frame_size, trackno, trackno_len);
        info->trackno = atoi(trackno);
    }
}

static int
_parse_id3v2(int fd, long id3v2_offset, struct lms_audio_info *info)
{
    char header_data[10], frame_header_data[10];
    unsigned int tag_size, major_version, frame_data_pos, frame_data_length, frame_header_size;
    int extended_header, footer_present;
    struct id3v2_frame_header fh;

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
        if (read(fd, frame_header_data, ID3V2_FRAME_HEADER_SIZE) != ID3V2_FRAME_HEADER_SIZE)
            return -1;

        if (frame_header_data[0] == 0)
            break;

        _parse_id3v2_frame_header(frame_header_data, major_version, &fh);

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

            _parse_id3v2_frame(&fh, frame_data, info);
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
_parse_id3v1(int fd, struct lms_audio_info *info)
{
    struct id3v1_tag tag;
    if (read(fd, &tag, sizeof(struct id3v1_tag)) == -1)
        return -1;

    info->title.str = strdup(tag.title);
    info->title.len = strlen(info->title.str);
    info->artist.str = strdup(tag.artist);
    info->artist.len = strlen(info->artist.str);
    info->album.str = strdup(tag.album);
    info->album.len = strlen(info->album.str);
    info->genre.str = strdup(id3v1_genres[(int) tag.genre]);
    info->genre.len = strlen(info->genre.str);
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
    struct lms_audio_info info = {0, {0}, {0}, {0}, {0}, 0, 0, 0};
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
        if (_parse_id3v2(fd, id3v2_offset, &info) != 0) {
            r = -2;
            goto done;
        }
    }
    else {
        char tag[3];
#if 0
        fprintf(stderr, "id3v2 tag not found in file %s. trying id3v1\n", finfo->path);
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
            if (_parse_id3v1(fd, &info) != 0) {
                r = -5;
                goto done;
            }
        }
    }

    lms_string_size_strip_and_free(&info.title);
    lms_string_size_strip_and_free(&info.artist);
    lms_string_size_strip_and_free(&info.album);
    lms_string_size_strip_and_free(&info.genre);

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
    if (info.album.str)
        lms_charset_conv(ctxt->cs_conv, &info.album.str, &info.album.len);
    if (info.genre.str)
        lms_charset_conv(ctxt->cs_conv, &info.genre.str, &info.genre.len);

#if 0
    fprintf(stderr, "file %s info\n", finfo->path);
    fprintf(stderr, "\ttitle='%s'\n", info.title.str);
    fprintf(stderr, "\tartist='%s'\n", info.artist.str);
    fprintf(stderr, "\talbum='%s'\n", info.album.str);
    fprintf(stderr, "\tgenre='%s'\n", info.genre.str);
    fprintf(stderr, "\ttrack number='%d'\n", info.trackno);
#endif

    info.id = finfo->id;
    r = lms_db_audio_add(plugin->audio_db, &info);

  done:
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
