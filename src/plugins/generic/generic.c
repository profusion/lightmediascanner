/**
 * Copyright (C) 2013 by Intel Corporation
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
 * @author Leandro Dorileo <leandro.maciel.dorileo@intel.com>
 */

#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include "libavutil/opt.h"

#include <lightmediascanner_db.h>
#include <lightmediascanner_plugin.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

static const char _name[] = "generic";

static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".mpeg"),
    LMS_STATIC_STRING_SIZE(".mpg"),
    LMS_STATIC_STRING_SIZE(".mpeg2"),
    LMS_STATIC_STRING_SIZE(".mp2"),
    LMS_STATIC_STRING_SIZE(".mp3"),
    LMS_STATIC_STRING_SIZE(".m3u"),
};

static const char *_cats[] = {
    "multimedia",
    "audio",
    "video",
    NULL
};

static const char *_authors[] = {
    "Leandro Dorileo",
    NULL
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
    lms_db_video_t *video_db;
};

struct mpeg_info {
    struct lms_string_size title;
    struct lms_string_size artist;
    struct lms_string_size album;
    struct lms_string_size genre;
};

#define DECL_STR(cname, str)                                            \
  static const struct lms_string_size cname = LMS_STATIC_STRING_SIZE(str) \

#define DLNA_VIDEO_RES(_width, _height)                                 \
    &(struct dlna_video_res) {.width = _width, .height = _height}       \

#define DLNA_VIDEO_RES_RANGE(_wmin, _wmax, _hmin, _hmax)                \
    &(struct dlna_video_res_range) {.width_min = _wmin,                 \
            .width_max = _wmax, .height_min = _hmin,                    \
            .height_max = _hmax}                                        \

#define DLNA_BITRATE(_min, _max)                            \
    &(struct dlna_bitrate) {.min = _min, .max = _max}       \

#define DLNA_LEVEL(_val...)				\
  &(struct dlna_level) {.levels = {_val, NULL}}		\

#define DLNA_PROFILE(_val...)				    \
  &(struct dlna_profile) {.profiles = {_val, NULL}}	    \

#define DLNA_AUDIO_RATE(_val...)				\
  &(struct dlna_audio_rate) {.rates = {_val, INT32_MAX}}	\

#define DLNA_VIDEO_PIXEL_ASPECT(_val...)				\
  &(struct dlna_video_pixel_aspect) {					\
    .pixel_aspect_ratio = {_val, NULL}}					\

#define DLNA_VIDEO_FRAMERATE(_val...)					\
  &(struct dlna_video_framerate) {.framerate = {_val, INT32_MAX}}	\

#define DLNA_VIDEO_PACKETSIZE(_packet_size)				\
  &(struct dlna_video_packet_size) {.packet_size = _packet_size}	\

#define MAX_AUDIO_MPEG_VERSIONS 2
#define MAX_AUDIO_RATES 9
#define MAX_AUDIO_LEVELS 5
#define MAX_VIDEO_RULE_LEVEL 12
#define MAX_VIDEO_RULE_PROFILE 3
#define MAX_VIDEO_RULE_FRAMERATE 7
#define MAX_VIDEO_PIXEL_ASPECT 3

struct dlna_bitrate {
     const unsigned int min;
     const unsigned int max;
};

struct dlna_video_res {
     const unsigned int width;
     const unsigned int height;
};

struct dlna_video_res_range {
    const unsigned int width_min;
    const unsigned int width_max;
    const unsigned int height_min;
    const unsigned int height_max;
};

struct dlna_level {
     const char *levels[MAX_VIDEO_RULE_LEVEL];
};

struct dlna_profile {
     const char *profiles[MAX_VIDEO_RULE_PROFILE];
};

struct dlna_video_framerate {
     const double framerate[MAX_VIDEO_RULE_FRAMERATE];
};

struct dlna_video_pixel_aspect {
     const char *pixel_aspect_ratio[MAX_VIDEO_PIXEL_ASPECT];
};

struct dlna_video_rule {
     const struct dlna_video_res *res;
     const struct dlna_video_res_range *res_range;
     const struct dlna_bitrate *bitrate;
     const struct dlna_profile *profiles;
     const struct dlna_level *levels;
     const struct dlna_video_framerate *framerate;
     const struct dlna_video_pixel_aspect *pixel_aspect;
};

struct dlna_audio_rate {
     const unsigned int rates[MAX_AUDIO_RATES];
};

struct dlna_audio_rule {
     const struct lms_string_size *codec;
     const struct lms_string_size *container;
     const struct dlna_audio_rate *rates;
     const struct dlna_level *levels;
     const struct dlna_bitrate *channels;
     const struct dlna_bitrate *bitrate;
};

struct dlna_video_packet_size {
     const int64_t packet_size;
};

struct dlna_video_profile {
     const struct lms_string_size *dlna_profile;
     const struct lms_string_size *dlna_mime;
     const struct dlna_video_rule *video_rules;
     const struct dlna_audio_rule *audio_rule;
     const struct lms_string_size *container;
     const struct dlna_video_packet_size *packet_size;
};

struct dlna_audio_profile {
     const struct lms_string_size *dlna_profile;
     const struct lms_string_size *dlna_mime;
     const struct dlna_audio_rule *audio_rule;
};

DECL_STR(_dlna_profile_mpeg_ts_sd_eu_iso, "MPEG_TS_SD_EU_ISO");
DECL_STR(_dlna_profile_mpeg_ts_sd_eu, "MPEG_TS_SD_EU");
DECL_STR(_dlna_profile_mpeg_ts_hd_na_iso, "MPEG_TS_HD_NA_ISO");
DECL_STR(_dlna_profile_mpeg_ts_hd_na, "MPEG_TS_HD_NA");
DECL_STR(_dlna_profile_mpeg_ts_sd_na_iso, "MPEG_TS_SD_NA_ISO");
DECL_STR(_dlna_profile_mpeg_ts_sd_na, "MPEG_TS_SD_NA");
DECL_STR(_dlna_profile_mpeg_ps_pal, "MPEG_PS_PAL");
DECL_STR(_dlna_profile_mpeg_ps_ntsc, "MPEG_PS_NTSC");
DECL_STR(_dlna_profile_mpeg1, "MPEG1");
DECL_STR(_dlna_profile_mp3, "MP3");
DECL_STR(_dlna_profile_mp3x, "MP3X");

DECL_STR(_dlna_mime_audio, "audio/mpeg");
DECL_STR(_dlna_mime_video, "video/mpeg");
DECL_STR(_dlna_mim_video_tts, "video/vnd.dlna.mpeg-tts");

DECL_STR(_container_mpegts, "mpegts");
DECL_STR(_container_mpeg, "mpeg");
DECL_STR(_container_mp3, "mp3");

static const struct dlna_video_rule _dlna_video_rule_mpeg_ps_pal[] = {
    {
        .res = DLNA_VIDEO_RES(720, 576),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main"),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("64:45", "16:15"),
    },
    {
        .res = DLNA_VIDEO_RES(704, 576),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main"),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("64:45", "16:15"),
    },
    {
        .res = DLNA_VIDEO_RES(544, 576),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main"),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("32:17", "24:17"),
    },
    {
        .res = DLNA_VIDEO_RES(480, 576),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main"),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("32:15", "8:5"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 576),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main"),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("32:11", "24:11"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 288),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main"),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("16:11", "12:11"),
    },
    { NULL },
};

static const struct dlna_video_rule _dlna_video_rule_mpeg1[] = {
    {
        .res = DLNA_VIDEO_RES(352, 288),
        .bitrate = DLNA_BITRATE(1150000, 1152000),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
    },
    {
        .res = DLNA_VIDEO_RES(352, 240),
        .bitrate = DLNA_BITRATE(1150000, 1152000),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001),
    },
    {
        .res = DLNA_VIDEO_RES(352, 240),
        .bitrate = DLNA_BITRATE(1150000, 1152000),
        .framerate = DLNA_VIDEO_FRAMERATE(24000/1001),
    },
    {
        .bitrate = DLNA_BITRATE(1150000, 1152000),
    },
    { NULL },
};

static const struct dlna_video_rule _dlna_video_rule_mpeg_ps_ntsc[] = {
    {
        .res = DLNA_VIDEO_RES(720, 480),
        .profiles = DLNA_PROFILE("simple", "main"),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("32:27", "8:9"),
    },
    {
        .res = DLNA_VIDEO_RES(704, 480),
        .profiles = DLNA_PROFILE("simple", "main"),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .framerate = DLNA_VIDEO_FRAMERATE(24000/1001, 24/1, 30000/1001, 30/1,
                    60000/1001, 60/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("40:33", "10:11"),
    },
    {
        .res = DLNA_VIDEO_RES(480, 480),
        .profiles = DLNA_PROFILE("simple", "main"),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("16:9", "4:3"),
    },
    {
        .res = DLNA_VIDEO_RES(544, 480),
        .profiles = DLNA_PROFILE("simple", "main"),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("80:51", "20:17"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 480),
        .profiles = DLNA_PROFILE("simple", "main"),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("80:33", "20:11"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 240),
        .profiles = DLNA_PROFILE("simple", "main"),
        .bitrate = DLNA_BITRATE(1, 9800000),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("16:11", "12:11"),
    },
    { NULL },
};

static const struct dlna_video_rule _dlna_video_rule_mpeg_ts_sd_na[] = {
    {
        .res = DLNA_VIDEO_RES(720, 480),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("32:27", "8:9"),
    },
    {
        .res = DLNA_VIDEO_RES(704, 480),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(24000/1001, 24/1, 30000/1001, 30/1,
                                          60000/1001, 60/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("40:33", "10:11"),
    },
    {
        .res = DLNA_VIDEO_RES(640, 480),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(24000/1001, 24/1, 30000/1001, 30/1,
                                          60000/1001, 60/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("1:1", "4:3"),
    },
    {
        .res = DLNA_VIDEO_RES(480, 480),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("16:9", "4:3"),
    },
    {
        .res = DLNA_VIDEO_RES(544, 480),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("80:51", "20:17"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 480),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("80:33", "20:11"),
    },
    { NULL },
};

static const struct dlna_video_rule _dlna_video_rule_mpeg_ts_hd_na[] = {
    {
        .res = DLNA_VIDEO_RES(1920, 1080),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001, 30/1, 24000/1001, 24/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("1:1", "9:16"),
    },
    {
        .res = DLNA_VIDEO_RES(1280, 720),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001, 30/1, 24000/1001, 24/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("1:1", "9:16"),
    },
    {
        .res = DLNA_VIDEO_RES(1080, 1440),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001, 30/1, 24000/1001, 24/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("4:3"),
    },
    {
        .res = DLNA_VIDEO_RES(1080, 1280),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(30000/1001, 30/1, 24000/1001, 24/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("3:2"),
    },
    { NULL },
};

static const struct dlna_video_rule _dlna_video_rule_mpeg_ts_sd_eu[] = {
    {
        .res = DLNA_VIDEO_RES(720, 576),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("64:45", "16:15"),
    },
    {
        .res = DLNA_VIDEO_RES(544, 576),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("32:17", "24:17"),
    },
    {
        .res = DLNA_VIDEO_RES(480, 576),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("32:15", "8:15"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 576),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("32:11", "24:11"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 288),
        .profiles = DLNA_PROFILE("simple", "main"),
        .levels = DLNA_LEVEL("low", "main", "high-1440", "high"),
        .bitrate = DLNA_BITRATE(1, 18881700),
        .framerate = DLNA_VIDEO_FRAMERATE(25/1),
        .pixel_aspect = DLNA_VIDEO_PIXEL_ASPECT("16:11", "12:11"),
    },
    { NULL },
};

static const struct dlna_audio_rule _dlna_audio_rule_mpeg_ps = {
    .rates = DLNA_AUDIO_RATE(48000),
};

static const struct dlna_audio_rule _dlna_audio_rule_mpeg1 = {
    .bitrate = DLNA_BITRATE(224000, 224000),
    .channels = DLNA_BITRATE(2, 2),
    .rates = DLNA_AUDIO_RATE(44100),
};

static const struct dlna_audio_rule _dlna_audio_rule_mpeg_ts_sd_hd_na = {
    .bitrate = DLNA_BITRATE(1, 448000),
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(48000),
};

static const struct dlna_audio_rule _dlna_audio_rule_mpeg_ts_sd_eu = {
     .bitrate = DLNA_BITRATE(1, 448000),
     .channels = DLNA_BITRATE(1, 6),
     .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
};

static const  struct dlna_video_profile _dlna_video_profile_rules[] = {
    {
        .dlna_profile = &_dlna_profile_mpeg_ps_pal,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg_ps_pal,
        .audio_rule = &_dlna_audio_rule_mpeg_ps,
        .container = &_container_mpeg,
    },
    {
        .dlna_profile = &_dlna_profile_mpeg1,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg1,
        .audio_rule = &_dlna_audio_rule_mpeg1,
        .container = &_container_mpeg,
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ps_ntsc,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg_ps_ntsc,
        .audio_rule = &_dlna_audio_rule_mpeg_ps,
        .container = &_container_mpeg,
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_sd_na,
        .dlna_mime = &_dlna_mim_video_tts,
        .video_rules = _dlna_video_rule_mpeg_ts_sd_na,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_hd_na,
        .packet_size = DLNA_VIDEO_PACKETSIZE(192),
        .container = &_container_mpegts,
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_sd_na_iso,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg_ts_sd_na,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_hd_na,
        .packet_size = DLNA_VIDEO_PACKETSIZE(188),
        .container = &_container_mpegts,
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_hd_na,
        .dlna_mime = &_dlna_mim_video_tts,
        .video_rules = _dlna_video_rule_mpeg_ts_hd_na,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_hd_na,
        .packet_size = DLNA_VIDEO_PACKETSIZE(192),
        .container = &_container_mpegts,
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_hd_na_iso,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg_ts_hd_na,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_hd_na,
        .packet_size = DLNA_VIDEO_PACKETSIZE(188),
        .container = &_container_mpegts,
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_sd_eu,
        .dlna_mime = &_dlna_mim_video_tts,
        .video_rules = _dlna_video_rule_mpeg_ts_sd_eu,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_eu,
        .packet_size = DLNA_VIDEO_PACKETSIZE(192),
        .container = &_container_mpegts,
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_sd_eu_iso,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg_ts_sd_eu,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_eu,
        .packet_size = DLNA_VIDEO_PACKETSIZE(188),
        .container = &_container_mpegts,
    },
};

static const struct dlna_audio_rule _dlna_audio_rule_mp3 = {
     .container = &_container_mp3,
     .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
     .bitrate = DLNA_BITRATE(320000, 320000),
     .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_rule_mp3x = {
     .container = &_container_mp3,
     .rates = DLNA_AUDIO_RATE(16000, 22050, 24000, 32000, 44100, 48000),
     .bitrate = DLNA_BITRATE(8000, 320000),
     .channels = DLNA_BITRATE(1, 2),
};

static const  struct dlna_audio_profile _dlna_audio_profile_rules[] = {
    {
        .dlna_profile = &_dlna_profile_mp3,
        .dlna_mime = &_dlna_mime_audio,
        .audio_rule = &_dlna_audio_rule_mp3,
    },
    {
        .dlna_profile = &_dlna_profile_mp3x,
        .dlna_mime = &_dlna_mime_audio,
        .audio_rule = &_dlna_audio_rule_mp3x,
    },
    {NULL},
};

static bool
_uint_vector_has_value(const unsigned int *list, unsigned int wanted)
{
    int i;
    unsigned int curr;

    for (i = 0, curr = list[i]; curr != INT32_MAX; i++, curr = list[i])
      if (curr == wanted) return true;

    return false;
}

static bool
_double_vector_has_value(const double *list, const double wanted)
{
    int i;
    double curr;

    for (i = 0, curr = list[i]; (int)curr != INT32_MAX; i++, curr = list[i]) {
      if (!(curr < wanted && curr > wanted)) return true;
    }

    return false;
}

static bool
_string_vector_has_value(const char **list, const char *wanted)
{
    int i;
    const char *curr;

    for (i = 0, curr = list[i]; curr != NULL; i++, curr = list[i]) {
        if (!strcmp(curr, wanted)) return true;
    }

    return false;
}

static const struct dlna_video_profile *
_get_video_dlna_profile(const struct lms_stream_audio_info *audio,
                        const struct lms_stream *audio_stream,
                        const struct lms_stream_video_info *video,
                        const struct lms_stream *video_stream,
                        int64_t packet_size,
                        struct lms_string_size *container)
{
    int i, length;
    const struct dlna_video_profile *curr;
    char *level, *profile, *profile_level;

    profile_level = strstr(video_stream->codec.str, "-p");
    level = strstr(profile_level, "-l");

    profile = calloc(1, sizeof(char) * (sizeof(level) - 2));
    strncpy(profile, profile_level, sizeof(level) - 2);

    length = sizeof(_dlna_video_profile_rules) / sizeof(struct dlna_video_profile);

    for (i = 0, curr = &_dlna_video_profile_rules[i]; i < length; i++,
             curr = &_dlna_video_profile_rules[i]) {
        const struct dlna_video_rule *video_rule;
        const struct dlna_audio_rule *audio_rule;
        const struct dlna_video_rule *r;

        audio_rule = curr->audio_rule;
        r = curr->video_rules;

        if (curr->container &&
            strcmp(curr->container->str, container->str))
            continue;

        if (curr->packet_size &&
            curr->packet_size->packet_size != packet_size)
            continue;

        if (audio) {

            if (audio_rule->bitrate &&
                (audio->bitrate < audio_rule->bitrate->min ||
                 audio->bitrate > audio_rule->bitrate->max)) {
                continue;
            }

            if (audio_rule->rates &&
                !_uint_vector_has_value(audio_rule->rates->rates,
                                        audio->sampling_rate))
                continue;

            if (audio_rule->channels &&
                (audio->channels < audio_rule->channels->min ||
                 audio->channels > audio_rule->channels->max))
                continue;
        }

        if (audio_stream && audio_rule->codec &&
            strcmp(audio_stream->codec.str, audio_rule->codec->str))
            continue;

        while (true) {
            video_rule = r++;

            if (!video_rule->res && !video_rule->bitrate && !video_rule->levels)
                break;

            if (video_rule->res && (video->width != video_rule->res->width &&
                                    video->height != video_rule->res->height))
                continue;

            if (video_rule->res_range &&
                !(video->width >= video_rule->res_range->width_min &&
                  video->width <= video_rule->res_range->width_max &&
                  video->height >= video_rule->res_range->height_min &&
                  video->height <= video_rule->res_range->height_max))
                continue;

            if (video_rule->framerate &&
                !_double_vector_has_value(video_rule->framerate->framerate,
                                          video->framerate))
                continue;

            if (video_rule->pixel_aspect && !_string_vector_has_value
                ((const char **)video_rule->pixel_aspect->pixel_aspect_ratio,
                 video->aspect_ratio.str))
                continue;

            if (video_rule->bitrate &&
                !(video->bitrate >= video_rule->bitrate->min &&
                  video->bitrate <= video_rule->bitrate->max))
                continue;

            if (video_rule->levels &&
                !_string_vector_has_value
                ((const char **)video_rule->levels->levels, level + 2))
                continue;

            if (video_rule->profiles &&
                !_string_vector_has_value
                ((const char **)video_rule->profiles->profiles, profile + 2))
                continue;

            free(profile);
            return curr;
        }
    }

    free(profile);
    return NULL;
}

static void
_fill_video_dlna_profile(struct lms_video_info *info,
                         int64_t packet_size,
                         struct lms_string_size *container)
{
    const struct dlna_video_profile *profile;
    const struct lms_stream *s, *audio_stream, *video_stream;
    const struct lms_stream_audio_info *audio;
    const struct lms_stream_video_info *video;

    audio_stream = video_stream = NULL;
    audio = NULL;
    video = NULL;

    for (s = info->streams; s; s = s->next) {
        if (s->type == LMS_STREAM_TYPE_VIDEO) {
            video = &s->video;
            video_stream = s;
            if (audio) break;
        } else if (s->type == LMS_STREAM_TYPE_AUDIO) {
            audio = &s->audio;
            audio_stream = s;
            if (video) break;
        }
    }

    profile = _get_video_dlna_profile(audio, audio_stream, video,
                                      video_stream, packet_size, container);
    if (!profile) return;

    info->dlna_profile = *profile->dlna_profile;
    info->dlna_mime = *profile->dlna_mime;
}

static void
_fill_audio_dlna_profile(struct lms_audio_info *info)
{
    int i = 0;

    while (true) {
      const struct dlna_audio_profile *curr = _dlna_audio_profile_rules + i;
      const struct dlna_audio_rule *rule;

      if (curr->dlna_profile == NULL && curr->dlna_mime == NULL &&
          curr->audio_rule == NULL)
        break;

      i++;
      rule = curr->audio_rule;

      if (rule->bitrate && info->bitrate &&
         (info->bitrate < rule->bitrate->min ||
          info->bitrate > rule->bitrate->max))
        continue;

      if (rule->rates &&
          !_uint_vector_has_value(rule->rates->rates,
                                  info->sampling_rate))
          continue;

      if (rule->channels &&
         (info->channels < rule->channels->min ||
          info->channels > rule->channels->max))
        continue;

      if (rule->codec && strcmp(rule->codec->str, info->codec.str))
        continue;

      if (rule->container && strcmp(rule->container->str, info->container.str))
        continue;

      info->dlna_mime = *curr->dlna_mime;
      info->dlna_profile = *curr->dlna_profile;
      break;
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

static char *
_get_dict_value(AVDictionary *dict, const char *key)
{
    AVDictionaryEntry *tag = NULL;
    tag = av_dict_get(dict, key, tag, AV_DICT_IGNORE_SUFFIX);
    if (!tag) return NULL;
    return tag->value;
}

static int
_parse(struct plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    int ret;
    int64_t packet_size = 0;
    unsigned int i;
    AVFormatContext *fmt_ctx = NULL;
    struct mpeg_info info = { };
    char *metadata;
    struct lms_audio_info audio_info = { };
    struct lms_video_info video_info = { };
    struct lms_string_size container = { };
    bool video = false;

    if ((ret = avformat_open_input(&fmt_ctx, finfo->path, NULL, NULL)))
        return ret;

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
    goto fail;

    metadata = _get_dict_value(fmt_ctx->metadata, "title");
    if (metadata)
        lms_string_size_strndup(&info.title, metadata, -1);

    metadata = _get_dict_value(fmt_ctx->metadata, "artist");
    if (metadata)
        lms_string_size_strndup(&info.artist, metadata, -1);

    metadata = _get_dict_value(fmt_ctx->metadata, "album");
    if (metadata)
        lms_string_size_strndup(&info.album, metadata, -1);

    metadata = _get_dict_value(fmt_ctx->metadata, "genre");
    if (metadata)
        lms_string_size_strndup(&info.genre, metadata, -1);

    av_opt_get_int(fmt_ctx, "ts_packetsize", AV_OPT_SEARCH_CHILDREN,
                   &packet_size);

    for (i = 0; i < fmt_ctx->nb_streams; i++) {
        AVStream *stream = fmt_ctx->streams[i];
        AVCodec *codec;

        if (stream->codec->codec_id == AV_CODEC_ID_PROBE) {
            printf("Failed to probe codec for input stream %d\n", stream->index);
        } else if (!(codec = avcodec_find_decoder(stream->codec->codec_id))) {
            printf("Unsupported codec with id %d for input stream %d\n",
                   stream->codec->codec_id, stream->index);
        } else {
            if (stream->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_info.bitrate = stream->codec->bit_rate;
                audio_info.channels = av_get_channel_layout_nb_channels
                  (stream->codec->channel_layout);
                audio_info.sampling_rate = stream->codec->sample_rate;
            } else if (stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
                const char *codec_name, *str_profile, *str_level;
                char buf[256], aspect_ratio[256];
                const char *language;
                struct lms_stream *s;

                s = calloc(1, sizeof(*s));
                s->stream_id = (unsigned int)stream->id;

                language = _get_dict_value(fmt_ctx->metadata, "language");
                if (metadata)
                  lms_string_size_strndup(&s->lang, language, -1);

                s->type = LMS_STREAM_TYPE_VIDEO;
                video = true;

                codec_name = avcodec_get_name(stream->codec->codec_id);
                if (codec_name) {
                    str_profile = NULL;
                    str_level = NULL;

                    switch (stream->codec->profile) {
                    case 6:
                        str_profile = "simple";
                        break;
                    case 4:
                        str_profile = "main";
                        break;
                    }

                    switch (stream->codec->level) {
                    case 5:
                    case 8:
                        str_level = "main";
                        break;
                    case 2:
                    case 4:
                        str_level = "high";
                        break;
                    case 6:
                        str_level = "high";
                        break;
                    }

                    snprintf(buf, sizeof(buf), "%s-p%s-l%s",
                             codec_name, str_profile, str_level);

                    lms_string_size_strndup(&s->codec, buf, -1);
                }

                s->video.bitrate = stream->codec->bit_rate;
                if (!s->video.bitrate)
                    s->video.bitrate = stream->codec->rc_max_rate;

                s->video.width = stream->codec->width;
                s->video.height = stream->codec->height;


                if (stream->r_frame_rate.den)
                    s->video.framerate = stream->r_frame_rate.num /
                        stream->r_frame_rate.den;

                snprintf(aspect_ratio, sizeof(aspect_ratio), "%d:%d",
                         stream->codec->sample_aspect_ratio.num,
                         stream->codec->sample_aspect_ratio.den);

                lms_string_size_strndup(&s->video.aspect_ratio,
                                        aspect_ratio, -1);

                s->next = video_info.streams;
                video_info.streams = s;
            }
        }
    }

    lms_string_size_strip_and_free(&info.title);
    lms_string_size_strip_and_free(&info.artist);
    lms_string_size_strip_and_free(&info.album);
    lms_string_size_strip_and_free(&info.genre);

    if (!info.title.str)
        lms_name_from_path(&info.title, finfo->path, finfo->path_len,
                           finfo->base, _exts[((long) match) - 1].len,
                           NULL);

    if (info.title.str)
        lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);
    if (info.artist.str)
        lms_charset_conv(ctxt->cs_conv, &info.artist.str, &info.artist.len);
    if (info.album.str)
      lms_charset_conv(ctxt->cs_conv, &info.album.str, &info.album.len);
    if (info.genre.str)
        lms_charset_conv(ctxt->cs_conv, &info.genre.str, &info.genre.len);


    lms_string_size_strndup(&container, fmt_ctx->iformat->name, -1);

    if (video) {
        video_info.id = finfo->id;
        video_info.title = info.title;
        video_info.artist = info.artist;
        video_info.container = container;

        _fill_video_dlna_profile(&video_info, packet_size, &container);
        ret = lms_db_video_add(plugin->video_db, &video_info);
    } else {
        audio_info.id = finfo->id;
        audio_info.title = info.title;
        audio_info.artist = info.artist;
        audio_info.album = info.album;
        audio_info.genre = info.genre;
        audio_info.container = container;

        _fill_audio_dlna_profile(&audio_info);
        ret = lms_db_audio_add(plugin->audio_db, &audio_info);
    }

    free(info.title.str);
    free(info.artist.str);
    free(info.album.str);
    free(info.genre.str);
    free(container.str);

    while (video_info.streams) {
        struct lms_stream *s = video_info.streams;
        video_info.streams = s->next;
        free(s->codec.str);
        if (s->type == LMS_STREAM_TYPE_VIDEO) {
            free(s->video.aspect_ratio.str);
        }
        free(s->lang.str);
        free(s);
    }

 fail:
    avformat_close_input(&fmt_ctx);
    return ret;
}

static int
_close(struct plugin *plugin)
{
    free(plugin);
    return 0;
}

static int
_setup(struct plugin *plugin,  struct lms_context *ctxt)
{
    av_register_all();
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
    plugin->plugin.order = INT32_MAX;

    return (struct lms_plugin *)plugin;
}

API const struct lms_plugin_info *
lms_plugin_info(void)
{
    static struct lms_plugin_info info = {
        _name,
        _cats,
        "libavcodec",
        PACKAGE_VERSION,
        _authors,
        "http://github.com/profusion/lightmediascanner"
    };

    return &info;
}
