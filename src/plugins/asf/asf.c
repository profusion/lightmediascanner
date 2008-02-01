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
 * asf/wma file parser.
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

enum StreamTypes {
    STREAM_TYPE_UNKNOWN = 0,
    STREAM_TYPE_AUDIO,
    STREAM_TYPE_VIDEO
};

enum AttributeTypes {
    ATTR_TYPE_UNICODE = 0,
    ATTR_TYPE_BYTES,
    ATTR_TYPE_BOOL,
    ATTR_TYPE_DWORD,
    ATTR_TYPE_QWORD,
    ATTR_TYPE_WORD,
    ATTR_TYPE_GUID
};

struct asf_info {
    struct lms_string_size title;
    struct lms_string_size artist;
    struct lms_string_size album;
    struct lms_string_size genre;
    unsigned char trackno;
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
    lms_db_video_t *video_db;
    lms_charset_conv_t *cs_conv;
};

static const char _name[] = "asf";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".wma"),
    LMS_STATIC_STRING_SIZE(".wmv"),
    LMS_STATIC_STRING_SIZE(".asf")
};

/* ASF GUIDs
 *
 * Microsoft defines these 16-byte (128-bit) GUIDs as:
 * first 8 bytes are in little-endian order
 * next 8 bytes are in big-endian order
 *
 * Eg.: AaBbCcDd-EeFf-GgHh-IiJj-KkLlMmNnOoPp:
 *
 * to convert to byte string do as follow:
 *
 * $Dd $Cc $Bb $Aa $Ff $Ee $Hh $Gg $Ii $Jj $Kk $Ll $Mm $Nn $Oo $Pp
 *
 * See http://www.microsoft.com/windows/windowsmedia/forpros/format/asfspec.aspx
 */
static const char header_guid[16] = "\x30\x26\xB2\x75\x8E\x66\xCF\x11\xA6\xD9\x00\xAA\x00\x62\xCE\x6C";
static const char file_properties_guid[16] = "\xA1\xDC\xAB\x8C\x47\xA9\xCF\x11\x8E\xE4\x00\xC0\x0C\x20\x53\x65";
static const char stream_properties_guid[16] = "\x91\x07\xDC\xB7\xB7\xA9\xCF\x11\x8E\xE6\x00\xC0\x0C\x20\x53\x65";
static const char stream_type_audio_guid[16] = "\x40\x9E\x69\xF8\x4D\x5B\xCF\x11\xA8\xFD\x00\x80\x5F\x5C\x44\x2B";
static const char stream_type_video_guid[16] = "\xC0\xEF\x19\xBC\x4D\x5B\xCF\x11\xA8\xFD\x00\x80\x5F\x5C\x44\x2B";
static const char content_description_guid[16] = "\x33\x26\xB2\x75\x8E\x66\xCF\x11\xA6\xD9\x00\xAA\x00\x62\xCE\x6C";
static const char extended_content_description_guid[16] = "\x40\xA4\xD0\xD2\x07\xE3\xD2\x11\x97\xF0\x00\xA0\xC9\x5E\xA8\x50";
static const char header_extension_guid[16] = "\xb5\x03\xbf_.\xa9\xcf\x11\x8e\xe3\x00\xc0\x0c Se";
static const char metadata_guid[16] = "\xEA\xCB\xF8\xC5\xAF[wH\204g\xAA\214D\xFAL\xCA";
static const char metadata_library_guid[16] = "\224\034#D\230\224\321I\241A\x1d\x13NEpT";
static const char content_encryption_object_guid[16] = "\xFB\xB3\x11\x22\x23\xBD\xD2\x11\xB4\xB7\x00\xA0\xC9\x55\xFC\x6E";
static const char extended_content_encryption_object_guid[16] = "\x14\xE6\x8A\x29\x22\x26\x17\x4C\xB9\x35\xDA\xE0\x7E\xE9\x28\x9C";

static const char attr_name_wm_album_title[26] = "\x57\x00\x4d\x00\x2f\x00\x41\x00\x6c\x00\x62\x00\x75\x00\x6d\x00\x54\x00\x69\x00\x74\x00\x6c\x00\x65\x00";
static const char attr_name_wm_genre[16] = "\x57\x00\x4d\x00\x2f\x00\x47\x00\x65\x00\x6e\x00\x72\x00\x65\x00";
static const char attr_name_wm_track_number[28] = "\x57\x00\x4d\x00\x2f\x00\x54\x00\x72\x00\x61\x00\x63\x00\x6b\x00\x4e\x00\x75\x00\x6d\x00\x62\x00\x65\x00\x72\x00";

static long long
_to_number(const char *data, unsigned int type_size, unsigned int data_size)
{
    long long sum = 0;
    unsigned int last, i;

    last = data_size > type_size ? type_size : data_size;

    for (i = 0; i < last; i++)
        sum |= (unsigned char) (data[i]) << (i * 8);

    return sum;
}

static short
_read_word(int fd)
{
    char v[2];
    if (read(fd, &v, 2) != 2)
        return 0;
    return (short) _to_number(v, sizeof(unsigned short), 2);
}

static unsigned int
_read_dword(int fd)
{
    char v[4];
    if (read(fd, &v, 4) != 4)
        return 0;
    return (unsigned int) _to_number(v, sizeof(unsigned int), 4);
}

static long long
_read_qword(int fd)
{
    char v[8];
    if (read(fd, &v, 8) != 8)
        return 0;
    return _to_number(v, sizeof(unsigned long long), 8);
}

static int
_read_string(int fd, size_t count, char **str, size_t *len)
{
    char *data;
    size_t data_size, size;

    if (!str) {
        lseek(fd, count, SEEK_CUR);
        return 0;
    }

    data = malloc(sizeof(char) * count);
    data_size = read(fd, data, count);
    if (data_size == -1) {
        free(data);
        return -1;
    }

    size = data_size;
    while (size >= 2) {
        if (data[size - 1] != '\0' || data[size - 2] != '\0')
            break;
        size -= 2;
    }

    *str = data;
    *len = size;

    return 0;
}

static void
_parse_content_description(lms_charset_conv_t *cs_conv, int fd, struct asf_info *info)
{
    int title_length = _read_word(fd);
    int artist_length = _read_word(fd);
    int copyright_length = _read_word(fd);
    int comment_length = _read_word(fd);
    int rating_length = _read_word(fd);

    _read_string(fd, title_length, &info->title.str, &info->title.len);
    lms_charset_conv_force(cs_conv, &info->title.str, &info->title.len);
    _read_string(fd, artist_length, &info->artist.str, &info->artist.len);
    lms_charset_conv_force(cs_conv, &info->artist.str, &info->artist.len);
    /* ignore copyright, comment and rating */
    lseek(fd, copyright_length + comment_length + rating_length, SEEK_CUR);
}

static void
_parse_attribute_name(int fd,
                      int kind,
                      char **attr_name,
                      size_t *attr_name_len,
                      int *attr_type,
                      int *attr_size)
{
    int attr_name_length;

    /* extended content descriptor */
    if (kind == 0) {
        attr_name_length = _read_word(fd);
        _read_string(fd, attr_name_length, attr_name, attr_name_len);
        *attr_type = _read_word(fd);
        *attr_size = _read_word(fd);
    }
    /* metadata & metadata library */
    else {
        lseek(fd, 2 + 2, SEEK_CUR); /* language and stream */
        attr_name_length = _read_word(fd);
        *attr_type = _read_word(fd);
        *attr_size = _read_dword(fd);
        _read_string(fd, attr_name_length, attr_name, attr_name_len);
    }
}

static void
_parse_attribute_string_data(lms_charset_conv_t *cs_conv,
                             int fd,
                             int attr_size,
                             char **attr_data,
                             size_t *attr_data_len)
{
    _read_string(fd, attr_size, attr_data, attr_data_len);
    lms_charset_conv_force(cs_conv, attr_data, attr_data_len);
}

static void
_skip_attribute_data(int fd, int kind, int attr_type, int attr_size)
{
    switch (attr_type) {
    case ATTR_TYPE_WORD:
        lseek(fd, 2, SEEK_CUR);
        break;

    case ATTR_TYPE_BOOL:
        if (kind == 0)
            lseek(fd, 4, SEEK_CUR);
        else
            lseek(fd, 2, SEEK_CUR);
        break;

    case ATTR_TYPE_DWORD:
        lseek(fd, 4, SEEK_CUR);
        break;

    case ATTR_TYPE_QWORD:
        lseek(fd, 8, SEEK_CUR);
        break;

    case ATTR_TYPE_UNICODE:
    case ATTR_TYPE_BYTES:
    case ATTR_TYPE_GUID:
        lseek(fd, attr_size, SEEK_CUR);
        break;

    default:
        break;
    }
}

static void
_skip_attribute(int fd, int kind)
{
    int attr_type, attr_size;
    _parse_attribute_name(fd, kind, NULL, NULL, &attr_type, &attr_size);
    _skip_attribute_data(fd, kind, attr_type, attr_size);
}

static void
_parse_extended_content_description_object(lms_charset_conv_t *cs_conv, int fd, struct asf_info *info)
{
    int count = _read_word(fd);
    char *attr_name;
    size_t attr_name_len;
    int attr_type, attr_size;
    while (count--) {
        attr_name = NULL;
        _parse_attribute_name(fd, 0,
                              &attr_name, &attr_name_len,
                              &attr_type, &attr_size);
        if (attr_type == ATTR_TYPE_UNICODE) {
            if (memcmp(attr_name, attr_name_wm_album_title, attr_name_len) == 0)
                _parse_attribute_string_data(cs_conv,
                                             fd, attr_size,
                                             &info->album.str,
                                             &info->album.len);
            else if (memcmp(attr_name, attr_name_wm_genre, attr_name_len) == 0)
                _parse_attribute_string_data(cs_conv,
                                             fd, attr_size,
                                             &info->genre.str,
                                             &info->genre.len);
            else if (memcmp(attr_name, attr_name_wm_track_number, attr_name_len) == 0) {
                char *trackno;
                size_t trackno_len;
                _parse_attribute_string_data(cs_conv,
                                             fd, attr_size,
                                             &trackno,
                                             &trackno_len);
                if (trackno) {
                    info->trackno = atoi(trackno);
                    free(trackno);
                }
            }
            else
                _skip_attribute_data(fd, 0, attr_type, attr_size);
        }
        else
            _skip_attribute_data(fd, 0, attr_type, attr_size);
        if (attr_name)
            free(attr_name);
    }
}

static void
_skip_metadata(int fd)
{
    int count = _read_word(fd);
    while (count--)
        _skip_attribute(fd, 1);
}

static void
_skip_metadata_library(int fd)
{
    int count = _read_word(fd);
    while (count--)
        _skip_attribute(fd, 2);
}

static void
_skip_header_extension(int fd)
{
    char guid[16];
    long long size, data_size, data_pos;

    lseek(fd, 18, SEEK_CUR);
    data_size = _read_dword(fd);
    data_pos = 0;
    while (data_pos < data_size) {
        read(fd, &guid, 16);
        size = _read_qword(fd);
        if (size == 0)
            break;
        if (memcmp(guid, metadata_guid, 16) == 0)
            _skip_metadata(fd);
        else if (memcmp(guid, metadata_library_guid, 16) == 0)
            _skip_metadata_library(fd);
        else
            lseek(fd, size - 24, SEEK_CUR);
        data_pos += size;
    }
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
    struct asf_info info = {{0}, {0}, {0}, {0}, 0};
    struct lms_audio_info audio_info = {0};
    struct lms_video_info video_info = {0};
    int r, fd, num_objects, i;
    char guid[16];
    unsigned int size;
    int stream_type = STREAM_TYPE_UNKNOWN;

    fd = open(finfo->path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (read(fd, &guid, 16) != 16) {
        perror("read");
        r = -2;
        goto done;
    }
    if (memcmp(guid, header_guid, 16) != 0) {
        fprintf(stderr, "ERROR: invalid header (%s).\n", finfo->path);
        r = -3;
        goto done;
    }

    size = _read_qword(fd);
    num_objects = _read_dword(fd);

    lseek(fd, 2, SEEK_CUR);

    for (i = 0; i < num_objects; ++i) {
        read(fd, &guid, 16);
        size = _read_qword(fd);

        if (memcmp(guid, file_properties_guid, 16) == 0)
            lseek(fd, size - 24, SEEK_CUR);
        else if (memcmp(guid, stream_properties_guid, 16) == 0) {
            read(fd, &guid, 16);
            if (memcmp(guid, stream_type_audio_guid, 16) == 0)
                stream_type = STREAM_TYPE_AUDIO;
            else if (memcmp(guid, stream_type_video_guid, 16) == 0)
                stream_type = STREAM_TYPE_VIDEO;
            lseek(fd, size - 40, SEEK_CUR);
        }
        else if (memcmp(guid, content_description_guid, 16) == 0)
            _parse_content_description(plugin->cs_conv, fd, &info);
        else if (memcmp(guid, extended_content_description_guid, 16) == 0)
            _parse_extended_content_description_object(plugin->cs_conv, fd, &info);
        else if (memcmp(guid, header_extension_guid, 16) == 0)
            _skip_header_extension(fd);
        else if (memcmp(guid, content_encryption_object_guid, 16) == 0 ||
                 memcmp(guid, extended_content_encryption_object_guid, 16) == 0) {
            /* ignore DRM'd files */
            fprintf(stderr, "ERROR: ignoring DRM'd file %s\n", finfo->path);
            r = -4;
            goto done;
        }
        else
            lseek(fd, size - 24, SEEK_CUR);
    }

    /* try to define stream type by extension */
    if (stream_type == STREAM_TYPE_UNKNOWN) {
        int ext_idx = ((int)match) - 1;
        if (strcmp(_exts[ext_idx].str, ".wma") == 0)
            stream_type = STREAM_TYPE_AUDIO;
        /* consider wmv and asf as video */
        else
            stream_type = STREAM_TYPE_VIDEO;
    }

    _strstrip(&info.title.str, &info.title.len);
    _strstrip(&info.artist.str, &info.artist.len);
    _strstrip(&info.album.str, &info.album.len);
    _strstrip(&info.genre.str, &info.genre.len);

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
    fprintf(stderr, "\ttrackno=%d\n", info.trackno);
#endif

    if (stream_type == STREAM_TYPE_AUDIO) {
        audio_info.id = finfo->id;
        audio_info.title = info.title;
        audio_info.artist = info.artist;
        audio_info.album = info.album;
        audio_info.genre = info.genre;
        audio_info.trackno = info.trackno;
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
    if (info.album.str)
        free(info.album.str);
    if (info.genre.str)
        free(info.genre.str);

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
    plugin->cs_conv = lms_charset_conv_new();
    if (!plugin->cs_conv)
        return -1;
    lms_charset_conv_add(plugin->cs_conv, "UTF-16LE");

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
    if (plugin->cs_conv)
        lms_charset_conv_free(plugin->cs_conv);

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
