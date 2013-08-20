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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * @author Andre Moreira Magalhaes <andre.magalhaes@openbossa.org>
 */

/**
 * @brief
 *
 * asf/wma file parser.
 *
 * Reference:
 *   http://www.microsoft.com/en-us/download/details.aspx?id=14995
 */

#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>
#include <shared/util.h>

#include <endian.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NSEC100_PER_SEC  10000000ULL
#define MSEC_PER_SEC  1000ULL

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
static const struct lms_string_size _container = LMS_STATIC_STRING_SIZE("asf");
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".wma"),
    LMS_STATIC_STRING_SIZE(".wmv"),
    LMS_STATIC_STRING_SIZE(".asf")
};
static const char *_cats[] = {
    "multimedia",
    "audio",
    NULL
};
static const char *_authors[] = {
    "Andre Moreira Magalhaes",
    NULL
};

struct stream {
    int id;
    enum StreamTypes type;
    struct stream *next;
    union {
        struct {
            uint16_t codec_id;
            uint16_t channels;
            uint32_t sampling_rate;
            uint32_t byterate;
        } audio;
    };
};

/* TODO: Add the gazillion of possible codecs -- possibly a task to gperf */
static const struct {
    uint16_t id;
    struct lms_string_size name;
} _codecs[] = {
    { 0x0160, LMS_STATIC_STRING_SIZE("wmav1") },
    { 0x0161, LMS_STATIC_STRING_SIZE("wmav2") },
    { 0x0162, LMS_STATIC_STRING_SIZE("wmavpro") },
    { 0x0163, LMS_STATIC_STRING_SIZE("wmavlossless") },
    { 0x1600, LMS_STATIC_STRING_SIZE("aac") },
    { 0x706d, LMS_STATIC_STRING_SIZE("aac") },
    { 0x4143, LMS_STATIC_STRING_SIZE("aac") },
    { 0xA106, LMS_STATIC_STRING_SIZE("aac") },
    { 0xF1AC, LMS_STATIC_STRING_SIZE("flac") },
    { 0x0055, LMS_STATIC_STRING_SIZE("mp3") },
    { }
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

static const char attr_name_wm_album_artist[28] = "\x57\x00\x4d\x00\x2f\x00\x41\x00\x6c\x00\x62\x00\x75\x00\x6d\x00\x41\x00\x72\x00\x74\x00\x69\x00\x73\x00\x74\x00";
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
_read_string(int fd, size_t count, char **str, unsigned int *len)
{
    char *data;
    ssize_t data_size, size;

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

static int
_parse_file_properties(lms_charset_conv_t *cs_conv, int fd,
                       struct lms_audio_info *info)
{
    struct {
        char fileid[16];
        uint64_t file_size;
        uint64_t creation_date;
        uint64_t data_packets_count;
        uint64_t play_duration;
        uint64_t send_duration;
        uint64_t preroll;
        uint32_t flags;
        uint32_t min_data_packet_size;
        uint32_t max_data_packet_size;
        uint32_t max_bitrate;
    } __attribute__((packed)) props;
    int r;

    r = read(fd, &props, sizeof(props));
    if (r != sizeof(props))
        return r;

    /* Broadcast flag */
    if (le32toh(props.flags) & 0x1)
        return r;

    /* ASF spec 01.20.06 sec. 3.2: we need to subtract the preroll value from
     * the duration in order to obtain the real duration */
    info->length = (unsigned int)((le64toh(props.play_duration) / NSEC100_PER_SEC)
                                  - le64toh(props.preroll) / MSEC_PER_SEC);

    return r;
}

static struct lms_string_size
_codec_id_to_str(uint16_t id)
{
    unsigned int i;

    for (i = 0; _codecs[i].name.str != NULL; i++)
        if (_codecs[i].id == id)
            return _codecs[i].name;

    return _codecs[i].name;
}

static int
_parse_stream_properties(int fd, struct stream **pstream)
{
    struct {
        char stream_type[16];
        char error_correction_type[16];
        uint64_t time_offset;
        uint32_t type_specific_len;
        uint32_t error_correction_data_len;
        uint16_t flags;
        uint32_t reserved; /* don't use, unaligned */
    }  __attribute__((packed)) props;

    int r;
    struct stream *s;

    *pstream = NULL;

    s = calloc(1, sizeof(struct stream));
    if (!s)
        return -ENOMEM;

    r = read(fd, &props, sizeof(props));
    if (r != sizeof(props))
        goto done;

    if (memcmp(props.stream_type, stream_type_audio_guid, 16) == 0)
        s->type = STREAM_TYPE_AUDIO;
    else if (memcmp(props.stream_type, stream_type_video_guid, 16) == 0)
        s->type = STREAM_TYPE_VIDEO;
    else {
        /* ignore stream */
        goto done;
    }

    s->id = le16toh(props.flags) & 0x7F;
    /* Not a valid stream */
    if (!s->id)
        goto done;

    if (s->type == STREAM_TYPE_AUDIO) {
        if (le32toh(props.type_specific_len) < 18)
            goto done;

        s->audio.codec_id = _read_word(fd);
        s->audio.channels = _read_word(fd);
        s->audio.sampling_rate = _read_dword(fd);
        s->audio.byterate = _read_dword(fd);
        r += 12;
    }

    *pstream = s;

    return r;

done:
    free(s);
    return r;
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
                      char **attr_name,
                      unsigned int *attr_name_len,
                      int *attr_type,
                      int *attr_size)
{
    int attr_name_length;

    attr_name_length = _read_word(fd);
    _read_string(fd, attr_name_length, attr_name, attr_name_len);
    *attr_type = _read_word(fd);
    *attr_size = _read_word(fd);
}

static void
_parse_attribute_string_data(lms_charset_conv_t *cs_conv,
                             int fd,
                             int attr_size,
                             char **attr_data,
                             unsigned int *attr_data_len)
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
_parse_extended_content_description_object(lms_charset_conv_t *cs_conv, int fd, struct asf_info *info)
{
    int count = _read_word(fd);
    char *attr_name;
    unsigned int attr_name_len;
    int attr_type, attr_size;
    while (count--) {
        attr_name = NULL;
        _parse_attribute_name(fd,
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
            else if (memcmp(attr_name, attr_name_wm_album_artist, attr_name_len) == 0)
                _parse_attribute_string_data(cs_conv,
                                             fd, attr_size,
                                             &info->artist.str,
                                             &info->artist.len);
            else if (memcmp(attr_name, attr_name_wm_track_number, attr_name_len) == 0) {
                char *trackno;
                unsigned int trackno_len;
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
        free(attr_name);
    }
}

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
_parse(struct plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    struct asf_info info = { };
    struct lms_audio_info audio_info = { };
    struct lms_video_info video_info = { };
    int r, fd, num_objects, i;
    char guid[16];
    unsigned int size;
    int stream_type = STREAM_TYPE_UNKNOWN;
    struct stream *streams = NULL;

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

        if (memcmp(guid, file_properties_guid, 16) == 0) {
            r = _parse_file_properties(plugin->cs_conv, fd, &audio_info);
            if (r < 0)
                goto done;
            lseek(fd, size - (24 + r), SEEK_CUR);
        } else if (memcmp(guid, stream_properties_guid, 16) == 0) {
            struct stream *s;
            r = _parse_stream_properties(fd, &s);
            if (r < 0)
                goto done;

            lseek(fd, size - (24 + r), SEEK_CUR);
            if (!s)
                continue;

            if (stream_type != STREAM_TYPE_VIDEO)
                stream_type = s->type;

            s->next = streams;
            streams = s;
        } else if (memcmp(guid, content_description_guid, 16) == 0)
            _parse_content_description(plugin->cs_conv, fd, &info);
        else if (memcmp(guid, extended_content_description_guid, 16) == 0)
            _parse_extended_content_description_object(plugin->cs_conv, fd, &info);
        else if (memcmp(guid, content_encryption_object_guid, 16) == 0 ||
                 memcmp(guid, extended_content_encryption_object_guid, 16) == 0) {
            /* ignore DRM'd files */
            fprintf(stderr, "ERROR: ignoring DRM'd file %s\n", finfo->path);
            r = -4;
            goto done;
        } else
            lseek(fd, size - 24, SEEK_CUR);
    }

    /* try to define stream type by extension */
    if (stream_type == STREAM_TYPE_UNKNOWN) {
        long ext_idx = ((long)match) - 1;
        if (strcmp(_exts[ext_idx].str, ".wma") == 0)
            stream_type = STREAM_TYPE_AUDIO;
        /* consider wmv and asf as video */
        else
            stream_type = STREAM_TYPE_VIDEO;
    }

    lms_string_size_strip_and_free(&info.title);
    lms_string_size_strip_and_free(&info.artist);
    lms_string_size_strip_and_free(&info.album);
    lms_string_size_strip_and_free(&info.genre);

    if (!info.title.str)
        info.title = str_extract_name_from_path(finfo->path, finfo->path_len,
                                                finfo->base,
                                                &_exts[((long) match) - 1],
                                                ctxt->cs_conv);

#if 0
    fprintf(stderr, "file %s info\n", finfo->path);
    fprintf(stderr, "\ttitle='%s'\n", info.title.str);
    fprintf(stderr, "\tartist='%s'\n", info.artist.str);
    fprintf(stderr, "\talbum='%s'\n", info.album.str);
    fprintf(stderr, "\tgenre='%s'\n", info.genre.str);
    fprintf(stderr, "\ttrackno=%d\n", info.trackno);
#endif

    audio_info.container = _container;

    if (stream_type == STREAM_TYPE_AUDIO) {
        audio_info.id = finfo->id;
        audio_info.title = info.title;
        audio_info.artist = info.artist;
        audio_info.album = info.album;
        audio_info.genre = info.genre;
        audio_info.trackno = info.trackno;

        audio_info.channels = streams->audio.channels;
        audio_info.bitrate = streams->audio.byterate * 8;
        audio_info.sampling_rate = streams->audio.sampling_rate;
        audio_info.codec = _codec_id_to_str(streams->audio.codec_id);

        r = lms_db_audio_add(plugin->audio_db, &audio_info);
    } else {
        video_info.id = finfo->id;
        video_info.title = info.title;
        video_info.artist = info.artist;
        r = lms_db_video_add(plugin->video_db, &video_info);
    }

done:
    while (streams) {
        struct stream *s = streams;
        streams = s->next;
        free(s);
    }

    free(info.title.str);
    free(info.artist.str);
    free(info.album.str);
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

API const struct lms_plugin_info *
lms_plugin_info(void)
{
    static struct lms_plugin_info info = {
        _name,
        _cats,
        "Microsoft WMA, WMV and ASF",
        PACKAGE_VERSION,
        _authors,
        "http://lms.garage.maemo.org"
    };

    return &info;
}
