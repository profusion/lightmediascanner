/**
 * Copyright (C) 2008-2011 by ProFUSION embedded systems
 * Copyright (C) 2007 by INdT
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
 * @author Gustavo Sverzut Barbieri <barbieri@profusion.mobi>
 */

/**
 * @brief
 *
 * Reads EXIF tags from images.
 *
 * @todo: get GPS data.
 * @todo: check if worth using mmap().
 */

#include <lightmediascanner_plugin.h>
#include <lightmediascanner_utils.h>
#include <lightmediascanner_db.h>
#include <shared/util.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const struct lms_string_size dlna_mime =
    LMS_STATIC_STRING_SIZE("image/jpeg");
static const struct lms_string_size dlna_sm_ico =
    LMS_STATIC_STRING_SIZE("JPEG_SM_ICO");
static const struct lms_string_size dlna_lrg_ico =
    LMS_STATIC_STRING_SIZE("JPEG_LRG_ICO");
static const struct lms_string_size dlna_tn =
    LMS_STATIC_STRING_SIZE("JPEG_TN");
static const struct lms_string_size dlna_sm =
    LMS_STATIC_STRING_SIZE("JPEG_SM");
static const struct lms_string_size dlna_med =
    LMS_STATIC_STRING_SIZE("JPEG_MED");
static const struct lms_string_size dlna_lrg =
    LMS_STATIC_STRING_SIZE("JPEG_LRG");

static void
_fill_dlna_profile(struct lms_image_info *info)
{
    const unsigned short w = info->width;
    const unsigned short h = info->height;

    info->dlna_mime = dlna_mime;

    if (w == 0 || h == 0)
        return;

    if (w == 48 && h == 48)
        info->dlna_profile = dlna_sm_ico;
    else if (w == 120 && h == 120)
        info->dlna_profile = dlna_lrg_ico;
    else if (w <= 160 && h <= 160)
        info->dlna_profile = dlna_tn;
    else if (w <= 640 && h <= 480)
        info->dlna_profile = dlna_sm;
    else if (w <= 1024 && h <= 768)
        info->dlna_profile = dlna_med;
    else if (w <= 4096 && h <= 4096)
        info->dlna_profile = dlna_lrg;
}

enum {
    JPEG_MARKER_SOI = 0xd8,
    JPEG_MARKER_DQT = 0xdb,
    JPEG_MARKER_JFIF = 0xe0,
    JPEG_MARKER_EXIF = 0xe1,
    JPEG_MARKER_COMM = 0xfe,
    JPEG_MARKER_SOF0 = 0xc0,
    JPEG_MARKER_SOF1 = 0xc1,
    JPEG_MARKER_SOF2 = 0xc2,
    JPEG_MARKER_SOF9 = 0xc9,
    JPEG_MARKER_SOF10 = 0xca,
    JPEG_MARKER_SOS = 0xda
};

/**
 * Process SOF JPEG, this contains width and height.
 */
static int
_jpeg_sof_process(int fd, unsigned short *width, unsigned short *height)
{
    unsigned char buf[6];

    if (read(fd, buf, 6) != 6) {
        perror("could not read() SOF data");
        return -1;
    }

    *height = (buf[1] << 8) | buf[2];
    *width = (buf[3] << 8) | buf[4];

    return 0;
}

/**
 * Process COM JPEG, this contains user comment.
 */
static int
_jpeg_com_process(int fd, int len, struct lms_string_size *comment)
{
    if (len < 1) {
        comment->str = NULL;
        comment->len = 0;
        return 0;
    }

    comment->str = malloc(len + 1);
    if (!comment->str) {
        perror("malloc");
        return -1;
    }
    if (read(fd, comment->str, len) != len) {
        perror("read");
        free(comment->str);
        comment->str = NULL;
        comment->len = 0;
        return -2;
    }
    if (comment->str[len - 1] == '\0')
        len--;
    else
        comment->str[len] = '\0';
    comment->len = len;

    lms_string_size_strip_and_free(comment);

    return 0;
}

/**
 * Walk JPEG markers in order to get useful information.
 */
static int
_jpeg_info_get(int fd, int len, struct lms_image_info *info)
{
    unsigned char buf[4];
    int found;
    off_t offset;

    found = info->title.str ? 1 : 0;
    offset = lseek(fd, len - 2, SEEK_CUR);
    len = 0;
    while (found < 2) {
        offset = lseek(fd, offset + len, SEEK_SET);
        if (offset == -1) {
            perror("lseek");
            return -1;
        }

        if (read(fd, buf, 4) != 4) {
            perror("read");
            return -2;
        }

        len = ((buf[2] << 8) | buf[3]) - 2;

        if (buf[0] != 0xff) {
            fprintf(stderr, "ERROR: expected 0xff marker, got %#x\n", buf[0]);
            return -3;
        }

        if (buf[1] == JPEG_MARKER_SOF0 ||
            buf[1] == JPEG_MARKER_SOF1 ||
            buf[1] == JPEG_MARKER_SOF2 ||
            buf[1] == JPEG_MARKER_SOF9 ||
            buf[1] == JPEG_MARKER_SOF10) {
            if (_jpeg_sof_process(fd, &info->width, &info->height) != 0)
                return -4;
            found++;
        } else if (buf[1] == JPEG_MARKER_COMM && !info->title.str) {
            if (_jpeg_com_process(fd, len, &info->title) != 0)
                return -5;
            found++;
        } else if (buf[1] == JPEG_MARKER_SOS)
            break;

        len += 4; /* add read size */
    }

    return 0;
}

/**
 * Read JPEG file start (0xffd8 marker) and return the next
 * marker type and its length.
 */
static int
_jpeg_data_get(int fd, int *type, int *len)
{
    unsigned char buf[6];

    if (lseek(fd, 0, SEEK_SET) != 0) {
        perror("lseek");
        return -1;
    }

    if (read(fd, buf, 6) != 6) {
        perror("read");
        return -2;
    }

    if (buf[0] != 0xff || buf[1] != JPEG_MARKER_SOI || buf[2] != 0xff) {
        fprintf(stderr, "ERROR: not JPEG file (magic=%#x %#x %#x)\n",
                buf[0], buf[1], buf[2]);
        return -3;
    }

    *type = buf[3];
    *len = (buf[4] << 8) | buf[5];

    return 0;
}

#define E_2BTYE(little_endian, a) ((little_endian) ? get_le16(a) : get_be16(a))
#define E_4BTYE(little_endian, a) ((little_endian) ? get_le32(a) : get_be32(a))

enum {
    EXIF_TYPE_BYTE = 1, /* 8 bit unsigned */
    EXIF_TYPE_ASCII = 2, /* 8 bit byte with 7-bit ASCII code, NULL terminated */
    EXIF_TYPE_SHORT = 3, /* 2-byte unsigned integer */
    EXIF_TYPE_LONG = 4, /* 4-byte unsigned integer */
    EXIF_TYPE_RATIONAL = 5, /* 2 4-byte unsigned integer, 1st = numerator */
    EXIF_TYPE_UNDEFINED = 7, /* 8-bit byte */
    EXIF_TYPE_SLONG = 9, /* 4-byte signed integer (2'complement) */
    EXIF_TYPE_SRATIONAL = 10 /* 2 4-byte signed integer, 1st = numerator */
};

enum {
    EXIF_TAG_ORIENTATION = 0x0112,
    EXIF_TAG_ARTIST = 0x013b,
    EXIF_TAG_USER_COMMENT = 0x9286,
    EXIF_TAG_IMAGE_DESCRIPTION = 0x010e,
    EXIF_TAG_DATE_TIME = 0x0132,
    EXIF_TAG_DATE_TIME_ORIGINAL = 0x9003,
    EXIF_TAG_DATE_TIME_DIGITIZED = 0x9004,
    EXIF_TAG_EXIF_IFD_POINTER = 0x8769
};


struct exif_ifd {
    unsigned short tag;
    unsigned short type;
    unsigned int count;
    unsigned int offset;
};

/**
 * Read IFD from stream.
 */
static int
_exif_ifd_get(int fd, int little_endian, struct exif_ifd *ifd)
{
    unsigned char buf[12];

    if (read(fd, buf, 12) != 12) {
        perror("read");
        return -1;
    }

    if (little_endian) {
        ifd->tag = get_le16(buf);
        ifd->type = get_le16(buf + 2);
        ifd->count = get_le32(buf + 4);
        ifd->offset = get_le32(buf + 8);
    } else {
        ifd->tag = get_be16(buf);
        ifd->type = get_be16(buf + 2);
        ifd->count = get_be32(buf + 4);
        ifd->offset = get_be32(buf + 8);
    }
    return 0;
}

/**
 * Get non-exif data based on Exif tag offset.
 *
 * This will setup the file description position and call _jpeg_info_get().
 */
static int
_exif_extra_get(int fd, int abs_offset, int len, struct lms_image_info *info)
{
    if (lseek(fd, abs_offset, SEEK_SET) == -1) {
        perror("lseek");
        return -1;
    }

    if (_jpeg_info_get(fd, len, info) != 0) {
        fprintf(stderr, "ERROR: could not get image size.\n");
        return -2;
    }
    return 0;
}

static int
_exif_text_encoding_get(int fd, unsigned int count, int offset, struct lms_string_size *s)
{
    if (count <= 8)
        return -1;

    count -= 8; /* XXX don't just ignore character code, handle it. */
    offset += 8;

    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("lseek");
        return -2;
    }

    s->str = malloc(count + 1);

    if (read(fd, s->str, count) != count) {
        perror("read");
        free(s->str);
        s->str = NULL;
        s->len = 0;
        return -3;
    }
    s->str[count] = '\0';
    s->len = count;

    lms_string_size_strip_and_free(s);

    return 0;
}

static int
_exif_text_ascii_get(int fd, unsigned int count, int offset, struct lms_string_size *s)
{
    if (count < 1) {
        s->str = NULL;
        s->len = 0;
        return 0;
    }

    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("lseek");
        return -1;
    }

    s->str = malloc(count);

    if (read(fd, s->str, count) != count) {
        perror("read");
        free(s->str);
        s->str = NULL;
        s->len = 0;
        return -1;
    }
    s->str[count - 1] = '\0';
    s->len = count - 1;

    lms_string_size_strip_and_free(s);

    return 0;
}

static unsigned int
_exif_datetime_get(int fd, int offset)
{
    char buf[20];
    struct tm tm = { };

    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("lseek");
        return 0;
    }

    if (read(fd, buf, 20) != 20) {
        perror("read");
        return 0;
    }

    buf[19] = '\0';
    if (strptime(buf, "%Y:%m:%d %H:%M:%S", &tm)) {
        return mktime(&tm);
    }
    return 0;
}

static int _exif_private_ifd_get(int fd, int base_offset, int offset, int little_endian, struct lms_image_info *info);

/**
 * Process IFD contents.
 */
static int
_exif_ifd_process(int fd, int count, int ifd_offset, int tiff_base, int little_endian, struct lms_image_info *info)
{
    int i, torig, tdig, tlast;

    torig = tdig = tlast = 0;

    for (i = 0; i < count; i++) {
        struct exif_ifd ifd;

        lseek(fd, tiff_base + ifd_offset + i * 12, SEEK_SET);
        if (_exif_ifd_get(fd, little_endian, &ifd) != 0) {
            fprintf(stderr, "ERROR: could not read Exif IFD.\n");
            return -8;
        }

        switch (ifd.tag) {
        case EXIF_TAG_ORIENTATION:
            info->orientation = ifd.offset >> 16;
            break;
        case EXIF_TAG_ARTIST:
            if (!info->artist.str)
                _exif_text_ascii_get(fd, ifd.count, tiff_base + ifd.offset,
                                     &info->artist);
            break;
        case EXIF_TAG_USER_COMMENT:
            if (!info->title.str)
                _exif_text_encoding_get(fd, ifd.count, tiff_base + ifd.offset,
                                        &info->title);
            break;
        case EXIF_TAG_IMAGE_DESCRIPTION:
            if (!info->title.str)
                _exif_text_ascii_get(fd, ifd.count, tiff_base + ifd.offset,
                                     &info->title);
            break;
        case EXIF_TAG_DATE_TIME:
            if (torig == 0 && info->date == 0)
                tlast = _exif_datetime_get(fd, tiff_base + ifd.offset);
            break;
        case EXIF_TAG_DATE_TIME_ORIGINAL:
            if (torig == 0 && info->date == 0)
                torig = _exif_datetime_get(fd, tiff_base + ifd.offset);
            break;
        case EXIF_TAG_DATE_TIME_DIGITIZED:
            if (torig == 0 && info->date == 0)
                tdig = _exif_datetime_get(fd, tiff_base + ifd.offset);
            break;
        case EXIF_TAG_EXIF_IFD_POINTER:
            if (ifd.count == 1 && ifd.type == EXIF_TYPE_LONG)
                _exif_private_ifd_get(fd, ifd.offset, tiff_base,
                                      little_endian, info);
            break;
        default:
            /* ignore */
            break;
        }
    }

    if (info->date == 0) {
        if (torig)
            info->date = torig;
        else if (tdig)
            info->date = tdig;
        else
            info->date = tlast;
    }

    return 0;
}

/**
 * Process Exif IFD (Exif Private Tag), with more specific info.
 */
static int
_exif_private_ifd_get(int fd, int ifd_offset, int tiff_base, int little_endian, struct lms_image_info *info)
{
    char buf[2];
    unsigned int count;

    if (lseek(fd, tiff_base + ifd_offset, SEEK_SET) == -1) {
        perror("lseek");
        return -1;
    }

    if (read(fd, buf, 2) != 2) {
        perror("read");
        return -1;
    }

    count = E_2BTYE(little_endian, buf);
    return _exif_ifd_process(fd, count, ifd_offset + 2, tiff_base,
                             little_endian, info);
}

/**
 * Process file as it being Exif, will extract Exif as well as other
 * JPEG markers (comment, size).
 */
static int
_exif_data_get(int fd, int len, struct lms_image_info *info)
{
    const unsigned char exif_hdr[6] = "Exif\0";
    unsigned char buf[8];
    unsigned int little_endian, offset, count;
    off_t abs_offset, tiff_base;

    abs_offset = lseek(fd, 0, SEEK_CUR);
    if (abs_offset == -1) {
        perror("lseek");
        return -1;
    }

    if (read(fd, buf, 6) != 6) {
        perror("read");
        return -2;
    }

    memset(info, 0, sizeof(*info));
    info->orientation = 1;

    if (memcmp(buf, exif_hdr, 6) != 0)
        return _exif_extra_get(fd, abs_offset, len, info);

    if (read(fd, buf, 8) != 8) {
        perror("read");
        return -4;
    }

    if (buf[0] == 'I' && buf[1] == 'I') {
        little_endian = 1;
        offset = get_le32(buf + 4);
    } else if (buf[0] == 'M' && buf[1] == 'M') {
        little_endian = 0;
        offset = get_be32(buf + 4);
    } else {
        fprintf(stderr, "ERROR: undefined byte sex \"%2.2s\".\n", buf);
        return -5;
    }

    offset -= 8;
    if (offset > 0 && lseek(fd, offset, SEEK_CUR) == -1) {
        perror("lseek");
        return -6;
    }

    tiff_base = abs_offset + 6; /* offsets are relative to TIFF base */

    if (read(fd, buf, 2) != 2) {
        perror("read");
        return -7;
    }
    count = E_2BTYE(little_endian, buf);

    _exif_ifd_process(fd, count, 8 + 2, tiff_base,
                      little_endian, info);

    return _exif_extra_get(fd, abs_offset, len, info);
}

/**
 * Process file as it being JFIF
 */
static int
_jfif_data_get(int fd, int len, struct lms_image_info *info)
{
    unsigned char buf[4];
    int new_len;

    memset(info, 0, sizeof(*info));
    info->orientation = 1;

    /* JFIF provides no useful information, try to find out Exif */
    if (lseek(fd, len - 2, SEEK_CUR) == -1) {
        perror("lseek");
        return -1;
    }

    if (read(fd, buf, 4) != 4) {
        perror("read");
        return -2;
    }

    new_len = ((buf[2] << 8) | buf[3]);
    if (buf[0] != 0xff) {
        fprintf(stderr, "ERROR: expected 0xff marker, got %#x\n", buf[0]);
        return -3;
    }

    if (buf[1] == JPEG_MARKER_EXIF)
        return _exif_data_get(fd, new_len, info);
    else {
        /* rollback to avoid losing initial frame */
        if (lseek(fd, - len - 2, SEEK_CUR) == -1) {
            perror("lseek");
            return -1;
        }
        return _jpeg_info_get(fd, len, info);
    }
}

static const char _name[] = "jpeg";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".jpg"),
    LMS_STATIC_STRING_SIZE(".jpeg"),
    LMS_STATIC_STRING_SIZE(".jpe")
};
static const char *_cats[] = {
    "multimedia",
    "picture",
    NULL
};
static const char *_authors[] = {
    "Gustavo Sverzut Barbieri",
    NULL
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_image_t *img_db;
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
_parse(struct plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    struct lms_image_info info = { };
    int fd, type, len, r;

    fd = open(finfo->path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (_jpeg_data_get(fd, &type, &len) != 0) {
        r = -2;
        goto done;
    }

    if (type == JPEG_MARKER_EXIF) {
        if (_exif_data_get(fd, len, &info) != 0) {
            fprintf(stderr, "ERROR: could not get EXIF info (%s).\n",
                    finfo->path);
            r = -3;
            goto done;
        }
    } else if (type == JPEG_MARKER_JFIF || type == JPEG_MARKER_DQT) {
        if (_jfif_data_get(fd, len, &info) != 0) {
            fprintf(stderr, "ERROR: could not get JPEG size (%s).\n",
                    finfo->path);
            r = -4;
            goto done;
        }
    } else {
        fprintf(stderr, "ERROR: unsupported JPEG marker %#x (%s)\n", type,
                finfo->path);
        r = -6;
        goto done;
    }

    if (info.date == 0)
        info.date = finfo->mtime;

    if (!info.title.str)
        info.title = str_extract_name_from_path(finfo->path, finfo->path_len,
                                                finfo->base,
                                                &_exts[((long) match) - 1],
                                                ctxt->cs_conv);
    if (info.artist.str)
      lms_charset_conv(ctxt->cs_conv, &info.artist.str, &info.artist.len);

    _fill_dlna_profile(&info);

    info.id = finfo->id;
    r = lms_db_image_add(plugin->img_db, &info);

  done:
    free(info.title.str);
    free(info.artist.str);

    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    close(fd);

    return r;
}

static int
_setup(struct plugin *plugin, struct lms_context *ctxt)
{
    plugin->img_db = lms_db_image_new(ctxt->db);
    if (!plugin->img_db)
        return -1;

    return 0;
}

static int
_start(struct plugin *plugin, struct lms_context *ctxt)
{
    return lms_db_image_start(plugin->img_db);
}

static int
_finish(struct plugin *plugin, struct lms_context *ctxt)
{
    if (plugin->img_db)
        return lms_db_image_free(plugin->img_db);

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

API const struct lms_plugin_info *
lms_plugin_info(void)
{
    static struct lms_plugin_info info = {
        _name,
        _cats,
        "JPEG pictures",
        PACKAGE_VERSION,
        _authors,
        "http://lms.garage.maemo.org"
    };

    return &info;
}
