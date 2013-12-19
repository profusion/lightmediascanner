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

#define DECL_STR(cname, str)                                            \
    static const struct lms_string_size cname = LMS_STATIC_STRING_SIZE(str)

DECL_STR(_codec_audio_wmav1, "wmav1");
DECL_STR(_codec_audio_wmav2, "wmav2");
DECL_STR(_codec_audio_wmavpro, "wmavpro");
DECL_STR(_codec_audio_wmavlossless, "wmavlossless");
DECL_STR(_codec_audio_aac, "aac");
DECL_STR(_codec_audio_flac, "flac");
DECL_STR(_codec_audio_mp3, "mp3");

DECL_STR(_codec_video_wmv1, "wmv1");
DECL_STR(_codec_video_wmv2, "wmv2");
DECL_STR(_codec_video_wmv3, "wmv3");

DECL_STR(_dlna_wma_base, "WMABASE");
DECL_STR(_dlna_wma_full, "WMAFULL");
DECL_STR(_dlna_wma_pro, "WMAPRO");
DECL_STR(_dlna_wma_mime, "audio/x-ms-wma");

DECL_STR(_str_unknown, "<UNKNOWN>");
#undef DECL_STR

static void
_fill_audio_dlna_profile(struct lms_audio_info *info)
{
    if ((info->codec.str == _codec_audio_wmav1.str ||
         info->codec.str == _codec_audio_wmav2.str) &&
        (info->sampling_rate <= 48000)) {
        info->dlna_mime = _dlna_wma_mime;
        if (info->bitrate <= 192999)
            info->dlna_profile = _dlna_wma_base;
        else
            info->dlna_profile = _dlna_wma_full;
    } else if (
        info->codec.str == _codec_audio_wmavpro.str &&
        info->sampling_rate <= 96000 &&
        info->channels <= 8 &&
        info->bitrate <= 1500000) {
        info->dlna_mime = _dlna_wma_mime;
        info->dlna_profile = _dlna_wma_pro;
    }
}

enum AttributeTypes {
    ATTR_TYPE_UNICODE = 0,
    ATTR_TYPE_BYTES,
    ATTR_TYPE_BOOL,
    ATTR_TYPE_DWORD,
    ATTR_TYPE_QWORD,
    ATTR_TYPE_WORD,
    ATTR_TYPE_GUID
};

struct stream {
    struct lms_stream base;
    struct {
        unsigned int sampling_rate;
        unsigned int bitrate;
        double framerate;
    } priv;
};

struct asf_info {
    struct lms_string_size title;
    struct lms_string_size artist;
    struct lms_string_size album;
    struct lms_string_size genre;
    enum lms_stream_type type;
    unsigned int length;
    unsigned char trackno;

    struct stream *streams;
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

/* TODO: Add the gazillion of possible codecs -- possibly a task to gperf */
static const struct {
    uint16_t id;
    const struct lms_string_size *name;
} _audio_codecs[] = {
    /* id == 0  is special, check callers if it's needed */
    { 0x0160, &_codec_audio_wmav1 },
    { 0x0161, &_codec_audio_wmav2 },
    { 0x0162, &_codec_audio_wmavpro },
    { 0x0163, &_codec_audio_wmavlossless },
    { 0x1600, &_codec_audio_aac },
    { 0x706d, &_codec_audio_aac },
    { 0x4143, &_codec_audio_aac },
    { 0xA106, &_codec_audio_aac },
    { 0xF1AC, &_codec_audio_flac },
    { 0x0055, &_codec_audio_mp3 },
    { 0x0, &_str_unknown }
};

/* TODO: Add the gazillion of possible codecs -- possibly a task to gperf */
static const struct {
    uint8_t id[4];
    const struct lms_string_size *name;
} _video_codecs[] = {
    /* id == 0  is special, check callers if it's needed */
    { "WMV1", &_codec_video_wmv1 },
    { "WMV2", &_codec_video_wmv2 },
    { "WMV3", &_codec_video_wmv3 },
    { "XXXX", &_str_unknown }
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
static const char header_extension_guid[16] = "\xB5\x03\xBF\x5F\x2E\xA9\xCF\x11\x8E\xE3\x00\xC0\x0C\x20\x53\x65";
static const char extended_stream_properties_guid[16] = "\xCB\xA5\xE6\x14\x72\xC6\x32\x43\x83\x99\xA9\x69\x52\x06\x5B\x5A";
static const char language_list_guid[16] = "\xA9\x46\x43\x7C\xE0\xEF\xFC\x4B\xB2\x29\x39\x3E\xDE\x41\x5C\x85";
static const char file_properties_guid[16] = "\xA1\xDC\xAB\x8C\x47\xA9\xCF\x11\x8E\xE4\x00\xC0\x0C\x20\x53\x65";
static const char stream_properties_guid[16] = "\x91\x07\xDC\xB7\xB7\xA9\xCF\x11\x8E\xE6\x00\xC0\x0C\x20\x53\x65";
static const char stream_type_audio_guid[16] = "\x40\x9E\x69\xF8\x4D\x5B\xCF\x11\xA8\xFD\x00\x80\x5F\x5C\x44\x2B";
static const char stream_type_video_guid[16] = "\xC0\xEF\x19\xBC\x4D\x5B\xCF\x11\xA8\xFD\x00\x80\x5F\x5C\x44\x2B";
static const char content_description_guid[16] = "\x33\x26\xB2\x75\x8E\x66\xCF\x11\xA6\xD9\x00\xAA\x00\x62\xCE\x6C";
static const char extended_content_description_guid[16] = "\x40\xA4\xD0\xD2\x07\xE3\xD2\x11\x97\xF0\x00\xA0\xC9\x5E\xA8\x50";
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
_parse_file_properties(int fd, struct asf_info *info)
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
    info->length = (unsigned int)(
        (le64toh(props.play_duration) / NSEC100_PER_SEC) -
        le64toh(props.preroll) / MSEC_PER_SEC);

    return r;
}

static const struct lms_string_size *
_audio_codec_id_to_str(uint16_t id)
{
    unsigned int i;

    for (i = 0; _audio_codecs[i].name != &_str_unknown; i++)
        if (_audio_codecs[i].id == id)
            return _audio_codecs[i].name;

    return _audio_codecs[i].name;
}

static const struct lms_string_size *
_video_codec_id_to_str(uint8_t id[4])
{
    unsigned int i;

    for (i = 0; _video_codecs[i].name != &_str_unknown; i++)
        if (memcmp(id, _video_codecs[i].id, 4) == 0)
            return _video_codecs[i].name;

    return _video_codecs[i].name;
}

static struct stream * _stream_get_or_create(struct asf_info *info,
                                             unsigned int stream_id)
{
    struct stream *s;

    for (s = info->streams; s; s = (struct stream *) s->base.next) {
        if (s->base.stream_id == stream_id)
            return s;
    }

    s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    /* The Stream Properties Object can be anywhere inside the Header Object:
     * before the Header Extension Object, after it or embedded into the
     * Extended Stream Properties, inside the Header Extension Object.
     *
     * When parsing we either create a new stream and prepend it to the list or
     * we return the one already created by a previous object (see the loop
     * above).
     *
     * Note that the stream type is only available in the Stream Properties
     * Object. A file with an Extended Stream Properties Object referring to a
     * stream that doesn't have a corresponding Stream Properties is invalid. We
     * let it into the list, but it won't have the stream_type set. In this case
     * LMS will end up ignoring the stream when we try to add the file in the
     * database -- this is why we set type to -1 here */
    s->base.stream_id = stream_id;
    s->base.type = -1;
    s->base.next = (struct lms_stream *) info->streams;
    info->streams = s;

    return s;
}

static void _stream_copy_extension_properties(struct stream *s)
{
    switch (s->base.type) {
    case LMS_STREAM_TYPE_AUDIO:
        s->base.audio.bitrate = s->priv.bitrate;
        break;
    case LMS_STREAM_TYPE_VIDEO:
        s->base.video.bitrate = s->priv.bitrate;
        s->base.video.framerate = s->priv.framerate;
        break;
    default:
        break;
    }
}

static int
_parse_stream_properties(int fd, struct asf_info *info)
{
    struct {
        char stream_type[16];
        char error_correction_type[16];
        uint64_t time_offset;
        uint32_t type_specific_len;
        uint32_t error_correction_data_len;
        uint16_t flags;
        uint32_t reserved; /* don't use, unaligned */
    } __attribute__((packed)) props;
    unsigned int stream_id;
    struct stream *s;
    int r, type;

    r = read(fd, &props, sizeof(props));
    if (r != sizeof(props))
        return r;

    stream_id = le16toh(props.flags) & 0x7F;

    /* Not a valid stream */
    if (!stream_id)
        return r;

    if (memcmp(props.stream_type, stream_type_audio_guid, 16) == 0)
        type = LMS_STREAM_TYPE_AUDIO;
    else if (memcmp(props.stream_type, stream_type_video_guid, 16) == 0)
        type = LMS_STREAM_TYPE_VIDEO;
    else
        /* ignore stream */
        return r;

    s = _stream_get_or_create(info, stream_id);
    if (!s)
        return -ENOMEM;

    s->base.type = type;

    if (s->base.type == LMS_STREAM_TYPE_AUDIO) {
        if (le32toh(props.type_specific_len) < 18)
            goto done;

        s->base.codec = *_audio_codec_id_to_str(_read_word(fd));
        s->base.audio.channels = _read_word(fd);
        s->priv.sampling_rate = _read_dword(fd);
        s->base.audio.sampling_rate = s->priv.sampling_rate;
        s->base.audio.bitrate = _read_dword(fd) * 8;
    } else {
        struct {
            uint32_t width_unused;
            uint32_t height_unused;
            uint8_t reserved;
            uint16_t data_size_unused;
            /* data */
            uint32_t size;
            uint32_t width;
            uint32_t height;
            uint16_t reseved2;
            uint16_t bits_per_pixel;
            uint8_t compression_id[4];
            uint32_t image_size;

            /* other fields are ignored */
        } __attribute__((packed)) video;

        r = read(fd, &video, sizeof(video));
        if (r != sizeof(video))
            goto done;

        if ((unsigned int) r < get_le32(&video.size) -
            (sizeof(video) - offsetof(typeof(video), width)))
            goto done;

        s->base.codec = *_video_codec_id_to_str(video.compression_id);
        s->base.video.width = get_le32(&video.width);
        s->base.video.height = get_le32(&video.height);
        lms_stream_video_info_aspect_ratio_guess(&s->base.video);
    }

    _stream_copy_extension_properties(s);

done:
    /* If there's any video stream, consider the file as video */
    if (info->type != LMS_STREAM_TYPE_VIDEO)
        info->type = s->base.type;

    return r;
}

static int _parse_extended_stream_properties(lms_charset_conv_t *cs_conv,
                                             int fd, struct asf_info *info)
{
    struct {
        uint64_t start_time;
        uint64_t end_time;
        uint32_t data_bitrate;
        uint32_t buffer_size;
        uint32_t init_buffer_fullness;
        uint32_t alt_data_bitrate;
        uint32_t alt_buffer_size;
        uint32_t alt_init_buffer_fullness;
        uint32_t max_obj_size;
        uint32_t flags;
        uint16_t stream_id;
        uint16_t lang_id;
        uint64_t avg_time_per_frame;
        uint16_t stream_name_count;
        uint16_t payload_extension_system_count;
    } __attribute__((packed)) props;
    struct stream *s;
    unsigned int stream_id;
    uint32_t bitrate;
    uint16_t n;
    int r;

    r = read(fd, &props, sizeof(props));
    if (r != sizeof(props))
        return r;

    stream_id = get_le16(&props.stream_id);
    s = _stream_get_or_create(info, stream_id);

    bitrate = get_le32(&props.alt_data_bitrate); /* for vbr */
    if (!bitrate)
        bitrate = get_le32(&props.data_bitrate);
    s->priv.bitrate = bitrate;
    s->priv.framerate = (NSEC100_PER_SEC /
                         (double) get_le64(&props.avg_time_per_frame));
    for (n = get_le16(&props.stream_name_count); n; n--) {
        uint16_t j;
        lseek(fd, 2, SEEK_CUR);
        j = _read_word(fd);
        lseek(fd, j, SEEK_CUR);
    }
    for (n = get_le16(&props.payload_extension_system_count); n; n--) {
        uint32_t j;
        lseek(fd, 18, SEEK_CUR);
        j = _read_dword(fd);
        lseek(fd, j, SEEK_CUR);
    }

    return 0;
}

/* Lazy implementation, let the parsing of subframes to the caller. Techically
 * this is wrong, since it might parse objects in the extension header that
 * should be in the header object, however this should parse ok all good files
 * and eventually the bad ones. */
static int _parse_header_extension(lms_charset_conv_t *cs_conv, int fd,
                                   struct asf_info *info)
{
    lseek(fd, 22, SEEK_CUR);
    return 0;
}

static int
_parse_content_description(lms_charset_conv_t *cs_conv, int fd,
                           struct asf_info *info)
{
    int title_length = _read_word(fd);
    int artist_length = _read_word(fd);

    lseek(fd, 6, SEEK_CUR);

    _read_string(fd, title_length, &info->title.str, &info->title.len);
    lms_charset_conv_force(cs_conv, &info->title.str, &info->title.len);
    _read_string(fd, artist_length, &info->artist.str, &info->artist.len);
    lms_charset_conv_force(cs_conv, &info->artist.str, &info->artist.len);

    /* ignore copyright, comment and rating */
    return 1;
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

static int
_parse_extended_content_description_object(lms_charset_conv_t *cs_conv, int fd,
                                           struct asf_info *info)
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

    return 1;
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

static void streams_free(struct stream *streams)
{
    while (streams) {
        struct stream *s = streams;
        streams = (struct stream *) s->base.next;

        switch (s->base.type) {
        case LMS_STREAM_TYPE_VIDEO:
            free(s->base.video.aspect_ratio.str);
            break;
        default:
            break;
        }

        free(s);
    }
}

/* TODO: Parse "Language List Object" (sec 4.6) which contains an array with all
 * the languages used (they are in UTF-16, so they need to be properly
 * converted). */
static int
_parse(struct plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    struct asf_info info = { .type = LMS_STREAM_TYPE_UNKNOWN };
    int r, fd;
    char guid[16];
    unsigned int size;
    unsigned long long hdrsize;
    off_t pos_end, pos = 0;

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

    hdrsize = _read_qword(fd);
    pos_end = lseek(fd, 6, SEEK_CUR) - 24 + hdrsize;

    while (1) {
        if (!pos)
            pos = lseek(fd, 0, SEEK_CUR);
        if (pos > pos_end - 24)
            break;

        read(fd, &guid, 16);
        size = _read_qword(fd);

        if (memcmp(guid, header_extension_guid, 16) == 0)
            r = _parse_header_extension(plugin->cs_conv, fd, &info);
        else if (memcmp(guid, extended_stream_properties_guid, 16) == 0)
            r = _parse_extended_stream_properties(plugin->cs_conv, fd, &info);
        else if (memcmp(guid, file_properties_guid, 16) == 0)
            r = _parse_file_properties(fd, &info);
        else if (memcmp(guid, stream_properties_guid, 16) == 0)
            r = _parse_stream_properties(fd, &info);
        else if (memcmp(guid, language_list_guid, 16) == 0)
            r = 1;
        else if (memcmp(guid, content_description_guid, 16) == 0)
            r = _parse_content_description(plugin->cs_conv, fd, &info);
        else if (memcmp(guid, extended_content_description_guid, 16) == 0)
            r = _parse_extended_content_description_object(plugin->cs_conv, fd,
                                                           &info);
        else if (memcmp(guid, content_encryption_object_guid, 16) == 0 ||
                 memcmp(guid, extended_content_encryption_object_guid, 16) == 0)
            /* ignore DRM'd files */
            r = -4;
        else
            r = 1;

        if (r < 0)
            goto done;

        if (r > 0)
            pos = lseek(fd, pos + size, SEEK_SET);
        else
            pos = 0;
    }

    /* try to define stream type by extension */
    if (info.type == LMS_STREAM_TYPE_UNKNOWN) {
        long ext_idx = ((long)match) - 1;
        if (strcmp(_exts[ext_idx].str, ".wma") == 0)
            info.type = LMS_STREAM_TYPE_AUDIO;
        /* consider wmv and asf as video */
        else
            info.type = LMS_STREAM_TYPE_VIDEO;
    }

    lms_string_size_strip_and_free(&info.title);
    lms_string_size_strip_and_free(&info.artist);
    lms_string_size_strip_and_free(&info.album);
    lms_string_size_strip_and_free(&info.genre);

    if (!info.title.str)
        lms_name_from_path(&info.title, finfo->path, finfo->path_len,
                           finfo->base, _exts[((long) match) - 1].len,
                           ctxt->cs_conv);

    if (info.type == LMS_STREAM_TYPE_AUDIO) {
        struct lms_audio_info audio_info = { };

        audio_info.id = finfo->id;
        audio_info.title = info.title;
        audio_info.artist = info.artist;
        audio_info.album = info.album;
        audio_info.genre = info.genre;
        audio_info.trackno = info.trackno;
        audio_info.length = info.length;
        audio_info.container = _container;

        /* ignore additional streams, use only the first one */
        if (info.streams) {
            struct stream *s = info.streams;
            audio_info.channels = s->base.audio.channels;
            audio_info.bitrate = s->base.audio.bitrate;
            audio_info.sampling_rate = s->priv.sampling_rate;
            audio_info.codec = s->base.codec;
        }

        _fill_audio_dlna_profile(&audio_info);

        r = lms_db_audio_add(plugin->audio_db, &audio_info);
    } else {
        struct lms_video_info video_info = { };

        video_info.id = finfo->id;
        video_info.title = info.title;
        video_info.artist = info.artist;
        video_info.length = info.length;
        video_info.container = _container;
        video_info.streams = (struct lms_stream *) info.streams;
        r = lms_db_video_add(plugin->video_db, &video_info);
    }

done:
    streams_free(info.streams);

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
    plugin->plugin.order = 0;

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
