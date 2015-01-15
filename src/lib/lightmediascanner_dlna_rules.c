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

#include <lightmediascanner_dlna.h>

#define DECL_STR(cname, str)                                            \
    static const struct lms_string_size cname = LMS_STATIC_STRING_SIZE(str)

/** MP3 **/
DECL_STR(_dlna_mime_audio, "audio/mpeg");
DECL_STR(_dlna_mime_adts, "audio/vnd.dlna.adts");

DECL_STR(_dlna_profile_mp3, "MP3");
DECL_STR(_dlna_profile_mp3x, "MP3X");

DECL_STR(_codec_mpeg2aac_lc, "mpeg2aac-lc");
DECL_STR(_codec_mpeg4aac_lc, "mpeg4aac-lc");

DECL_STR(_dlna_profile_aac_adts_320, "AAC_ADTS_320");
DECL_STR(_dlna_profile_aac_adts, "AAC_ADTS");
DECL_STR(_dlna_profile_aac_adts_mult5, "AAC_MULT5_ADTS");


static const struct lms_dlna_audio_rule _dlna_audio_rule_mpeg2_aac_adts_320 = {
    .codec = &_codec_mpeg2aac_lc,
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(0, 320000),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_mpeg2_aac_adts = {
    .codec = &_codec_mpeg2aac_lc,
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(0, 576000),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_mpeg2_aac_adts_mult5 = {
    .codec = &_codec_mpeg2aac_lc,
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(0, 1440000),
    .channels = DLNA_BITRATE(1, 6),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_mpeg4_aac_adts_320 = {
    .codec = &_codec_mpeg4aac_lc,
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(0, 320000),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_mpeg4_aac_adts = {
    .codec = &_codec_mpeg4aac_lc,
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(0, 576000),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_mpeg4_aac_adts_mult5 = {
    .codec = &_codec_mpeg4aac_lc,
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(0, 1440000),
    .channels = DLNA_BITRATE(1, 6),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_mp3 = {
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(320000, 320000),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_mp3x = {
    .rates = DLNA_AUDIO_RATE(16000, 22050, 24000, 32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(8000, 320000),
    .channels = DLNA_BITRATE(1, 2),
};

const struct lms_dlna_audio_profile _dlna_mp3_rules[] = {
    {
        .dlna_profile = &_dlna_profile_aac_adts_320,
        .dlna_mime = &_dlna_mime_adts,
        .audio_rule = &_dlna_audio_rule_mpeg2_aac_adts_320,
    },
    {
        .dlna_profile = &_dlna_profile_aac_adts,
        .dlna_mime = &_dlna_mime_adts,
        .audio_rule = &_dlna_audio_rule_mpeg2_aac_adts,
    },
    {
        .dlna_profile = &_dlna_profile_aac_adts_mult5,
        .dlna_mime = &_dlna_mime_adts,
        .audio_rule = &_dlna_audio_rule_mpeg2_aac_adts_mult5,
    },
    {
        .dlna_profile = &_dlna_profile_aac_adts_320,
        .dlna_mime = &_dlna_mime_adts,
        .audio_rule = &_dlna_audio_rule_mpeg4_aac_adts_320,
    },
    {
        .dlna_profile = &_dlna_profile_aac_adts,
        .dlna_mime = &_dlna_mime_adts,
        .audio_rule = &_dlna_audio_rule_mpeg4_aac_adts,
    },
    {
        .dlna_profile = &_dlna_profile_aac_adts_mult5,
        .dlna_mime = &_dlna_mime_adts,
        .audio_rule = &_dlna_audio_rule_mpeg4_aac_adts_mult5,
    },
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

/** MPEG (1/2) **/
DECL_STR(_dlna_profile_mpeg_ts_sd_eu_iso, "MPEG_TS_SD_EU_ISO");
DECL_STR(_dlna_profile_mpeg_ts_sd_eu, "MPEG_TS_SD_EU");
DECL_STR(_dlna_profile_mpeg_ts_hd_na_iso, "MPEG_TS_HD_NA_ISO");
DECL_STR(_dlna_profile_mpeg_ts_hd_na, "MPEG_TS_HD_NA");
DECL_STR(_dlna_profile_mpeg_ts_sd_na_iso, "MPEG_TS_SD_NA_ISO");
DECL_STR(_dlna_profile_mpeg_ts_sd_na, "MPEG_TS_SD_NA");
DECL_STR(_dlna_profile_mpeg_ps_pal, "MPEG_PS_PAL");
DECL_STR(_dlna_profile_mpeg_ps_ntsc, "MPEG_PS_NTSC");
DECL_STR(_dlna_profile_mpeg1, "MPEG1");

DECL_STR(_dlna_mime_video, "video/mpeg");
DECL_STR(_dlna_mim_video_tts, "video/vnd.dlna.mpeg-tts");

static const struct lms_dlna_video_rule _dlna_video_rule_mpeg_ps_pal[] = {
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

static const struct lms_dlna_video_rule _dlna_video_rule_mpeg1[] = {
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

static const struct lms_dlna_video_rule _dlna_video_rule_mpeg_ps_ntsc[] = {
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

static const struct lms_dlna_video_rule _dlna_video_rule_mpeg_ts_sd_na[] = {
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

static const struct lms_dlna_video_rule _dlna_video_rule_mpeg_ts_hd_na[] = {
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

static const struct lms_dlna_video_rule _dlna_video_rule_mpeg_ts_sd_eu[] = {
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

static const struct lms_dlna_audio_rule _dlna_audio_rule_mpeg_ps = {
    .rates = DLNA_AUDIO_RATE(48000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_mpeg1 = {
    .bitrate = DLNA_BITRATE(224000, 224000),
    .channels = DLNA_BITRATE(2, 2),
    .rates = DLNA_AUDIO_RATE(44100),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_mpeg_ts_sd_hd_na = {
    .bitrate = DLNA_BITRATE(1, 448000),
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(48000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_mpeg_ts_sd_eu = {
    .bitrate = DLNA_BITRATE(1, 448000),
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
};

// rules
const struct lms_dlna_video_profile _dlna_mpeg_rules[] = {
    {
        .dlna_profile = &_dlna_profile_mpeg_ps_pal,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg_ps_pal,
        .audio_rule = &_dlna_audio_rule_mpeg_ps,
    },
    {
        .dlna_profile = &_dlna_profile_mpeg1,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg1,
        .audio_rule = &_dlna_audio_rule_mpeg1,
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ps_ntsc,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg_ps_ntsc,
        .audio_rule = &_dlna_audio_rule_mpeg_ps,
    },
    {NULL}
};

const struct lms_dlna_video_profile _dlna_mpegts_rules[] = {
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_sd_na,
        .dlna_mime = &_dlna_mim_video_tts,
        .video_rules = _dlna_video_rule_mpeg_ts_sd_na,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_hd_na,
        .packet_size = DLNA_VIDEO_PACKETSIZE(192),
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_sd_na_iso,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg_ts_sd_na,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_hd_na,
        .packet_size = DLNA_VIDEO_PACKETSIZE(188),
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_hd_na,
        .dlna_mime = &_dlna_mim_video_tts,
        .video_rules = _dlna_video_rule_mpeg_ts_hd_na,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_hd_na,
        .packet_size = DLNA_VIDEO_PACKETSIZE(192),
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_hd_na_iso,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg_ts_hd_na,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_hd_na,
        .packet_size = DLNA_VIDEO_PACKETSIZE(188),
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_sd_eu,
        .dlna_mime = &_dlna_mim_video_tts,
        .video_rules = _dlna_video_rule_mpeg_ts_sd_eu,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_eu,
        .packet_size = DLNA_VIDEO_PACKETSIZE(192),
    },
    {
        .dlna_profile = &_dlna_profile_mpeg_ts_sd_eu_iso,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_mpeg_ts_sd_eu,
        .audio_rule = &_dlna_audio_rule_mpeg_ts_sd_eu,
        .packet_size = DLNA_VIDEO_PACKETSIZE(188),
    },
    {NULL}
};

/** MP4 */

DECL_STR(_container_mp4, "mp4");
DECL_STR(_container_3gp, "3gp");

DECL_STR(_codec_audio_mpeg4aac_lc, "mpeg4aac-lc");
DECL_STR(_codec_audio_mpeg4aac_ltp, "mpeg4aac-ltp");
DECL_STR(_codec_audio_amr, "amr");
DECL_STR(_codec_audio_amr_wb, "amr-wb");

DECL_STR(_dlna_profile_p2_sp_aac, "MPEG4_P2_MP4_SP_AAC");
DECL_STR(_dlna_profile_p2_sp_aac_ltp, "MPEG4_P2_MP4_SP_AAC_LTP");
DECL_STR(_dlna_profile_p2_sp_vga_aac, "MPEG4_P2_MP4_SP_VGA_AAC");
DECL_STR(_dlna_profile_p2_sp_l2_aac, "MPEG4_P2_MP4_SP_L2_AAC");
DECL_STR(_dlna_profile_p2_sp_l5_aac, "MPEG4_P2_MP4_SP_L5_AAC");
DECL_STR(_dlna_profile_p2_sp_l6_aac, "MPEG4_P2_MP4_SP_L6_AAC");
DECL_STR(_dlna_profile_h263_p0_l10_aac, "MPEG4_H263_MP4_P0_L10_AAC");
DECL_STR(_dlna_profile_h263_p0_l10_aac_ltp, "MPEG4_H263_MP4_P0_L10_AAC_LTP");
DECL_STR(_dlna_profile_avc_mp4_bl_cif15_aac_520, "AVC_MP4_BL_CIF15_AAC_520");
DECL_STR(_dlna_profile_avc_mp4_bl_cif15_aac, "AVC_MP4_BL_CIF15_AAC");
DECL_STR(_dlna_profile_avc_mp4_bl_l3l_sd_aac, "AVC_MP4_BL_L3L_SD_AAC");
DECL_STR(_dlna_profile_avc_mp4_bl_l3_sd_aac, "AVC_MP4_BL_L3_SD_AAC");
DECL_STR(_dlna_profile_avc_mp4_mp_sd_aac_mult5, "AVC_MP4_MP_SD_AAC_MULT5");
DECL_STR(_dlna_profile_avc_mp4_mp_sd_mpeg1_l3, "AVC_MP4_MP_SD_MPEG1_L3");
DECL_STR(_dlna_profile_avc_mp4_mp_sd_ac3, "AVC_MP4_MP_SD_AC3");
DECL_STR(_dlna_profile_avc_mp4_mp_sd_eac3, "AVC_MP4_MP_SD_EAC3");
DECL_STR(_dlna_profile_avc_mp4_mp_hd_720p_aac, "AVC_MP4_MP_HD_720p_AAC");
DECL_STR(_dlna_profile_avc_mkv_mp_hd_aac_mult5, "AVC_MKV_MP_HD_AAC_MULT5");
DECL_STR(_dlna_profile_avc_mkv_hp_hd_aac_mult5, "AVC_MKV_HP_HD_AAC_MULT5");
DECL_STR(_dlna_profile_avc_mkv_mp_hd_ac3, "AVC_MKV_MP_HD_AC3");
DECL_STR(_dlna_profile_avc_mkv_hp_hd_ac3, "AVC_MKV_HP_HD_AC3");
DECL_STR(_dlna_profile_avc_mkv_mp_hd_mpeg1_l3, "AVC_MKV_MP_HD_MPEG1_L3");
DECL_STR(_dlna_profile_avc_mkv_hp_hd_mpeg1_l3, "AVC_MKV_HP_HD_MPEG1_L3");

DECL_STR(_dlna_profile_aac_iso, "AAC_ISO");
DECL_STR(_dlna_profile_aac_iso_320, "AAC_ISO_320");
DECL_STR(_dlna_profile_aac_mult5_iso, "AAC_MULT5_ISO");
DECL_STR(_dlna_profile_ac3, "AC3");
DECL_STR(_dlna_profile_eac3, "EAC3");
DECL_STR(_dlna_profile_amr_3gpp, "AMR_3GPP");
DECL_STR(_dlna_profile_amr_wbplus, "AMR_WBplus");

DECL_STR(_dlna_mime_video_3gp, "video/3gpp");
DECL_STR(_dlna_mime_video_matroska, "video/x-matroska");

DECL_STR(_dlna_mime_audio_3gp, "audio/3gpp");
DECL_STR(_dlna_mime_audio_dolby, "audio/vnd.dolby.dd-raw");
DECL_STR(_dlna_mime_audio_eac3, "audio/eac3");

static const struct lms_dlna_video_rule _dlna_video_rule_sp_l3[] = {
    {
        .res = DLNA_VIDEO_RES(352, 288),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 240),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(320, 240),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(320, 180),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1")
    },
    {
        .res = DLNA_VIDEO_RES(240, 180),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(208, 160),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(176, 144),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(176, 120),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 120),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 112),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 90),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(128, 96),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 288),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 240),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(320, 240),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(320, 180),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(240, 180),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(208, 160),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(176, 144),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(176, 120),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 120),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 112),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 90),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(128, 96),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 288),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 240),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(320, 240),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(320, 180),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(240, 180),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(208, 160),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(176, 155),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(176, 120),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 120),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 112),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 90),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(128, 96),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    { NULL },
};

#define DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD                         \
    "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3"      \

static const struct lms_dlna_video_rule _dlna_video_rule_avc_mp4_mp_sd[] = {
    {
        .res = DLNA_VIDEO_RES(720, 576),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(720, 480),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(704, 576),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(704, 480),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(704, 480),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(640, 480),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(640, 360),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(544, 576),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(544, 480),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(480, 576),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(480, 480),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(480, 360),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(480, 270),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(352, 576),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(352, 480),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(352, 288),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(352, 240),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(320, 240),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(320, 180),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(240, 180),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(208, 160),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(176, 144),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(176, 120),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(150, 120),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(160, 112),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(160, 90),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    {
        .res = DLNA_VIDEO_RES(128, 96),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MP4_MP_SD),
    },
    { NULL },
};

#define DLNA_VIDEO_LEVELS_MPEG4_P2_MP4_SP_VGA_AAC       \
    "0", "0b", "1", "2", "3"                            \

static const struct lms_dlna_video_rule _dlna_video_mpeg4_p2_mp4_sp_vga_aac[] = {
    {
        .res = DLNA_VIDEO_RES(640, 480),
        .bitrate = DLNA_BITRATE(1, 3000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_MPEG4_P2_MP4_SP_VGA_AAC),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(-1, -1),
    },
    {
        .res = DLNA_VIDEO_RES(640, 360),
        .bitrate = DLNA_BITRATE(1, 3000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_MPEG4_P2_MP4_SP_VGA_AAC),
    },
    { NULL },
};

#define DLNA_VIDEO_LEVELS_MPEG4_P2_MP4_SP_L2_ACC        \
    "0", "0b", "1", "2"                                 \

static const struct lms_dlna_video_rule _dlna_video_mpeg4_p2_mp4_sp_l2_aac[] = {
    {
        .res = DLNA_VIDEO_RES(352, 288),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_MPEG4_P2_MP4_SP_L2_ACC),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 15/1),
    },
    {
        .res = DLNA_VIDEO_RES(320, 240),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_MPEG4_P2_MP4_SP_L2_ACC),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 15/1),
    },
    {
        .res = DLNA_VIDEO_RES(320, 180),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_MPEG4_P2_MP4_SP_L2_ACC),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 15/1),
    },
    {
        .res = DLNA_VIDEO_RES(176, 144),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_MPEG4_P2_MP4_SP_L2_ACC),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),
    },
    {
        .res = DLNA_VIDEO_RES(128, 96),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_MPEG4_P2_MP4_SP_L2_ACC),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),
    },
    { NULL },
};

#define DLNA_VIDEO_RULE_SP_L5                           \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 480),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 360),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 576),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 480),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 288),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 240),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 240),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 180),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(240, 180),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(208, 160),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 144),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 120),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 120),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 112),                \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 90),                 \
            .bitrate = DLNA_BITRATE(1, 64000),          \
            .levels = DLNA_LEVEL("0", "1"),             \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 480),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 360),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 576),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 480),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 288),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 240),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 240),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 180),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(240, 180),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(208, 160),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 144),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 120),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 120),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 112),                \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 90),                 \
            .bitrate = DLNA_BITRATE(1, 128000),         \
            .levels = DLNA_LEVEL("0b", "2"),            \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 480),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 360),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 576),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 480),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 288),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 240),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 240),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 180),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(240, 180),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(208, 160),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 144),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 120),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 120),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 112),                \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 90),                 \
            .bitrate = DLNA_BITRATE(1, 384000),         \
            .levels = DLNA_LEVEL("3"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 480),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5"),                  \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 360),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 576),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 480),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 288),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 240),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 240),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 180),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(240, 180),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(208, 160),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 144),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 120),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 120),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 112),                \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            },                                          \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 90),                 \
            .bitrate = DLNA_BITRATE(1, 8000000),        \
            .levels = DLNA_LEVEL("5")                   \
            }                                           \

static const struct lms_dlna_video_rule _dlna_video_rule_sp_l5[] = {
    DLNA_VIDEO_RULE_SP_L5,
    { NULL },
};

static const struct lms_dlna_video_rule _dlna_video_rule_sp_l6[] = {
    DLNA_VIDEO_RULE_SP_L5,
    {
        .res = DLNA_VIDEO_RES(1280, 720),
        .bitrate = DLNA_BITRATE(1, 64000),
        .levels = DLNA_LEVEL("0", "1"),
    },
    {
        .res = DLNA_VIDEO_RES(1280, 720),
        .bitrate = DLNA_BITRATE(1, 128000),
        .levels = DLNA_LEVEL("0b", "2"),
    },
    {
        .res = DLNA_VIDEO_RES(1280, 720),
        .bitrate = DLNA_BITRATE(1, 384000),
        .levels = DLNA_LEVEL("3"),
    },
    {
        .res = DLNA_VIDEO_RES(640, 480),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(720, 576),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(720, 480),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 288),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 240),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(320, 240),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(320, 180),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(240, 180),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(208, 160),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(176, 144),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(176, 120),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 120),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 112),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 90),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(1280, 720),
        .bitrate = DLNA_BITRATE(1, 4000000),
        .levels = DLNA_LEVEL("4a"),
    },
    {
        .res = DLNA_VIDEO_RES(1280, 720),
        .bitrate = DLNA_BITRATE(1, 8000000),
        .levels = DLNA_LEVEL("5"),
    },
    {
        .res = DLNA_VIDEO_RES(640, 480),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(640, 360),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(720, 576),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(720, 480),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 288),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(352, 240),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(320, 240),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(320, 180),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(240, 180),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(208, 160),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(176, 144),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(176, 120),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 120),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 112),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(160, 90),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    {
        .res = DLNA_VIDEO_RES(1280, 720),
        .bitrate = DLNA_BITRATE(1, 12000000),
        .levels = DLNA_LEVEL("6"),
    },
    { NULL },
};

#define DLNA_VIDEO_RULE_H263_P0_L10             \
    {                                           \
        .res = DLNA_VIDEO_RES(176, 144),        \
            .bitrate = DLNA_BITRATE(1, 64000),  \
            .levels = DLNA_LEVEL("0")           \
            },                                  \
    {                                           \
        .res = DLNA_VIDEO_RES(128, 96),         \
            .bitrate = DLNA_BITRATE(1, 64000),  \
            .levels = DLNA_LEVEL("0")           \
            }                                   \

static const struct lms_dlna_video_rule _dlna_video_rule_mpeg4_h263_mp4_p0_l10_aac[] = {
DLNA_VIDEO_RULE_H263_P0_L10,
{ NULL },
    };

static const struct lms_dlna_video_rule _dlna_video_rule_mpeg4_h263_mp4_p0_l10_aac_ltp[] = {
DLNA_VIDEO_RULE_H263_P0_L10,
{ NULL },
    };

#define DLNA_VIDEO_LEVELS_CIF                   \
    "1", "1b", "1.1", "1.2"                     \

#define DLNA_VIDEO_RULES_AVC_MP4_BL_CIF15                               \
    {                                                                   \
     .res = DLNA_VIDEO_RES(352, 288),                                   \
          .bitrate = DLNA_BITRATE(1, 384000),                           \
          .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                  \
          .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 15/1),     \
          },                                                            \
    {                                                                   \
     .res = DLNA_VIDEO_RES(352, 240),                                   \
          .bitrate = DLNA_BITRATE(1, 384000),                           \
          .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                  \
          .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 18/1),     \
          },                                                            \
    {                                                                   \
     .res = DLNA_VIDEO_RES(320, 240),                                   \
          .bitrate = DLNA_BITRATE(1, 384000),                           \
          .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                  \
          .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 20/1),     \
          },                                                            \
    {                                                                   \
     .res = DLNA_VIDEO_RES(320, 180),                                   \
          .bitrate = DLNA_BITRATE(1, 384000),                           \
          .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                  \
          .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 26/1),     \
          },                                                            \
    {                                                                   \
     .res = DLNA_VIDEO_RES(240, 180),                                   \
          .bitrate = DLNA_BITRATE(1, 384000),                           \
          .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                  \
          .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),     \
          },                                                            \
          {                                                             \
           .res = DLNA_VIDEO_RES(208, 160),                             \
                .bitrate = DLNA_BITRATE(1, 384000),                     \
                .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),            \
                .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1), \
                },                                                      \
          {                                                             \
           .res = DLNA_VIDEO_RES(176, 144),                             \
                .bitrate = DLNA_BITRATE(1, 384000),                     \
                .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),            \
                .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1), \
                },                                                      \
          {                                                             \
           .res = DLNA_VIDEO_RES(176, 120),                             \
                .bitrate = DLNA_BITRATE(1, 384000),                     \
                .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),            \
                .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1), \
                },                                                      \
          {                                                             \
           .res = DLNA_VIDEO_RES(160, 120),                             \
                .bitrate = DLNA_BITRATE(1, 384000),                     \
                .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),            \
                .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1), \
                },                                                      \
          {                                                             \
           .res = DLNA_VIDEO_RES(160, 112),                             \
                .bitrate = DLNA_BITRATE(1, 384000),                     \
                .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),            \
                .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1), \
                },                                                      \
                {                                                       \
                 .res = DLNA_VIDEO_RES(160, 90),                        \
                      .bitrate = DLNA_BITRATE(1, 384000),               \
                      .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),      \
                      .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1), \
                      },                                                \
                {                                                       \
                 .res = DLNA_VIDEO_RES(128, 96),                        \
                      .bitrate = DLNA_BITRATE(1, 384000),               \
                      .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),      \
                      .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1), \
                      },                                                \
                {                                                       \
                 .res = DLNA_VIDEO_RES(240, 135),                       \
                      .bitrate = DLNA_BITRATE(1, 384000),               \
                      .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),      \
                      .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1), \
                      }                                                 \

static const struct lms_dlna_video_rule _dlna_video_rule_avc_mp4_bl_cif15_aac_520[] = {
    DLNA_VIDEO_RULES_AVC_MP4_BL_CIF15,
    { NULL },
};

static const struct lms_dlna_video_rule _dlna_video_rule_avc_mp4_bl_cif15_aac[] = {
    DLNA_VIDEO_RULES_AVC_MP4_BL_CIF15,
    { NULL },
};

#define DLNA_VIDEO_RULES_BL_L3L_SD_AAC                          \
    "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3"      \

// common between l3 and l3l sd aac
#define DLNA_VIDEO_RULES_L3_L3L_SD_AAC                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(720, 576),                                \
            .bitrate = DLNA_BITRATE(1, 4500000),                        \
            .levels = DLNA_LEVEL(DLNA_VIDEO_RULES_BL_L3L_SD_AAC),       \
            .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 25/1),   \
            },                                                          \
    {                                                                   \
        .res = DLNA_VIDEO_RES(720, 480),                                \
            .bitrate = DLNA_BITRATE(1, 4500000),                        \
            .levels = DLNA_LEVEL(DLNA_VIDEO_RULES_BL_L3L_SD_AAC),       \
            .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30000/1001), \
            },                                                          \
    {                                                                   \
        .res = DLNA_VIDEO_RES(640, 480),                                \
            .bitrate = DLNA_BITRATE(1, 4500000),                        \
            .levels = DLNA_LEVEL(DLNA_VIDEO_RULES_BL_L3L_SD_AAC),       \
            .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),   \
            },                                                          \
    {                                                                   \
        .res = DLNA_VIDEO_RES(640, 360),                                \
            .bitrate = DLNA_BITRATE(1, 4500000),                        \
            .levels = DLNA_LEVEL(DLNA_VIDEO_RULES_BL_L3L_SD_AAC),       \
            .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),   \
            }                                                           \

static const struct lms_dlna_video_rule _dlna_video_rule_avc_mp4_bl_l3l_sd_aac[] = {
    DLNA_VIDEO_RULES_L3_L3L_SD_AAC,
    { NULL },
};

static const struct lms_dlna_video_rule _dlna_video_rule_avc_mp4_bl_l3_sd_aac[] = {
    DLNA_VIDEO_RULES_L3_L3L_SD_AAC,
    { NULL },
};

#define DLNA_VIDEO_LEVELS_SD_EAC3                               \
    "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3"      \

static const struct lms_dlna_video_rule _dlna_video_rule_avc_mp4_mp_sd_eac3[] = {
    {
        .res = DLNA_VIDEO_RES(864, 480),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_SD_EAC3),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 25/1),
    },
    {
        .res = DLNA_VIDEO_RES(720, 576),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_SD_EAC3),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 50/1),
    },
    {
        .res = DLNA_VIDEO_RES(720, 480),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_SD_EAC3),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 60/1),
    },
    {
        .res = DLNA_VIDEO_RES(640, 480),
        .bitrate = DLNA_BITRATE(1, 10000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_SD_EAC3),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),
    },
    {NULL},
};

#define DLNA_VIDEO_LEVELS_MP_HD_720P                                    \
    "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3", "3.1"       \

static const struct lms_dlna_video_rule _dlna_video_rule_avc_mp4_mp_hd_720p_aac[] = {
    {
        .res = DLNA_VIDEO_RES(1280, 720),
        .bitrate = DLNA_BITRATE(1, 14000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_MP_HD_720P),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),
    },
    {
        .res = DLNA_VIDEO_RES(640, 480),
        .bitrate = DLNA_BITRATE(1, 14000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_MP_HD_720P),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 60/1),
    },
    {NULL},
};

#define DLNA_VIDEO_LEVELS_MP_HD_1080I                                   \
    "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3", "3.1", "3.2", "4" \

static const struct lms_dlna_video_rule _dlna_video_rule_avc_mp4_mp_hd_1080i_aac[] = {
    {
        .res = DLNA_VIDEO_RES(1920, 1080),
        .bitrate = DLNA_BITRATE(1, 20000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_MP_HD_1080I),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),
    },
    {
        .res = DLNA_VIDEO_RES(1280, 720),
        .bitrate = DLNA_BITRATE(1, 20000000),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_MP_HD_1080I),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 60/1),
    },
    {NULL},
};

#define DLNA_VIDEO_LEVELS_AVC_MKV_HP_HD                                 \
    "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3", "3.1", "3.2", "4" \

static const struct lms_dlna_video_rule _dlna_video_avc_mkv_hp_hd[] = {
    {
        .res_range = DLNA_VIDEO_RES_RANGE(1, 1920, 1, 1152),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MKV_HP_HD),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 60/1),
    },
    {
        .res_range = DLNA_VIDEO_RES_RANGE(1, 1920, 1, 1152),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MKV_HP_HD),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 50/1),
    },
    {
        .res_range = DLNA_VIDEO_RES_RANGE(1, 1920, 1, 1152),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MKV_HP_HD),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 25/1),
    },
    {
        .res_range = DLNA_VIDEO_RES_RANGE(1, 1920, 1, 1080),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MKV_HP_HD),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),
    },
    {
        .res_range = DLNA_VIDEO_RES_RANGE(1, 1920, 1, 1080),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MKV_HP_HD),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 60/1),
    },
    {
        .res_range = DLNA_VIDEO_RES_RANGE(1, 1280, 1, 720),
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_AVC_MKV_HP_HD),
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 60/1),
    },
    {NULL},
};

static const struct lms_dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 216000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_aac_ltp = {
    .codec = &_codec_audio_mpeg4aac_ltp,
    .bitrate = DLNA_BITRATE(1, 216000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_vga_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 256000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_l2_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 128000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_l5_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_l6_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_mpeg4_h263_mp4_p0_l10_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 86000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_mpeg4_h263_mp4_p0_l10_aac_ltp = {
    .codec = &_codec_audio_mpeg4aac_ltp,
    .bitrate = DLNA_BITRATE(1, 86000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

// avc
static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mp4_bl_cif15_aac_520 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 128000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mp4_bl_cif15_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 200000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mp4_bl_l3l_sd_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 256000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mp4_bl_l3_sd_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 256000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_sd_aac_mult5 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0,1440000),
    .levels = DLNA_LEVEL("1", "2", "4"),
    .channels = DLNA_BITRATE(1, 6),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_sd_mpeg1_l3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(32000, 32000),
    .channels = DLNA_BITRATE(1, 2),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_sd_ac3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(64000, 64000),
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_sd_eac3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0, 3024000),
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_hd_720p_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0, 576000),
    .channels = DLNA_BITRATE(1, 2),
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_hd_1080i_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0, 576000),
    .channels = DLNA_BITRATE(1, 2),
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mkv_mp_hd_aac_mult5 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0, 1440000),
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
    .levels = DLNA_LEVEL("1", "2", "4"),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mkv_mp_hd_ac3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0, 1440000),
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_avc_mkv_mp_hd_mpeg1_l3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .channels = DLNA_BITRATE(1, 2),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(32000, 320000),
};

const struct lms_dlna_video_profile _dlna_mp4_video_rules[] = {
    // mpeg4
    {
        .dlna_profile = &_dlna_profile_p2_sp_aac,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_sp_l3,
        .audio_rule = &_dlna_audio_mpeg4_p2_mp4_sp_aac,
    },
    {
        .dlna_profile = &_dlna_profile_p2_sp_aac_ltp,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_sp_l3,
        .audio_rule = &_dlna_audio_mpeg4_p2_mp4_sp_aac_ltp,
    },
    {
        .dlna_profile = &_dlna_profile_p2_sp_vga_aac,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_mpeg4_p2_mp4_sp_vga_aac,
        .audio_rule = &_dlna_audio_mpeg4_p2_mp4_sp_vga_aac,
    },
    {
        .dlna_profile = &_dlna_profile_p2_sp_l2_aac,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_mpeg4_p2_mp4_sp_l2_aac,
        .audio_rule = &_dlna_audio_mpeg4_p2_mp4_sp_l2_aac,
    },
    {
        .dlna_profile = &_dlna_profile_p2_sp_l5_aac,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_sp_l5,
        .audio_rule = &_dlna_audio_mpeg4_p2_mp4_sp_l5_aac,
    },
    {
        .dlna_profile = &_dlna_profile_p2_sp_l6_aac,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_sp_l6,
        .audio_rule = &_dlna_audio_mpeg4_p2_mp4_sp_l6_aac,
    },
    {
        .dlna_profile = &_dlna_profile_h263_p0_l10_aac,
        .dlna_mime = &_dlna_mime_video_3gp,
        .video_rules = _dlna_video_rule_mpeg4_h263_mp4_p0_l10_aac,
        .audio_rule = &_dlna_audio_mpeg4_h263_mp4_p0_l10_aac,
    },
    {
        .dlna_profile = &_dlna_profile_h263_p0_l10_aac_ltp,
        .dlna_mime = &_dlna_mime_video_3gp,
        .video_rules = _dlna_video_rule_mpeg4_h263_mp4_p0_l10_aac_ltp,
        .audio_rule = &_dlna_audio_mpeg4_h263_mp4_p0_l10_aac_ltp,
    },
    // avc
    {
        .dlna_profile = &_dlna_profile_avc_mp4_bl_cif15_aac_520,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_avc_mp4_bl_cif15_aac_520,
        .audio_rule = &_dlna_audio_rule_avc_mp4_bl_cif15_aac_520,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mp4_bl_cif15_aac,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_avc_mp4_bl_cif15_aac,
        .audio_rule = &_dlna_audio_rule_avc_mp4_bl_cif15_aac,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mp4_bl_l3l_sd_aac,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_avc_mp4_bl_l3l_sd_aac,
        .audio_rule = &_dlna_audio_rule_avc_mp4_bl_l3l_sd_aac,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mp4_bl_l3_sd_aac,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_avc_mp4_bl_l3_sd_aac,
        .audio_rule = &_dlna_audio_rule_avc_mp4_bl_l3_sd_aac,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mp4_mp_sd_aac_mult5,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_avc_mp4_mp_sd,
        .audio_rule = &_dlna_audio_rule_avc_mp4_mp_sd_aac_mult5,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mp4_mp_sd_mpeg1_l3,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_avc_mp4_mp_sd,
        .audio_rule = &_dlna_audio_rule_avc_mp4_mp_sd_mpeg1_l3,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mp4_mp_sd_ac3,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_avc_mp4_mp_sd,
        .audio_rule = &_dlna_audio_rule_avc_mp4_mp_sd_ac3,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mp4_mp_sd_eac3,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_avc_mp4_mp_sd_eac3,
        .audio_rule = &_dlna_audio_rule_avc_mp4_mp_sd_eac3,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mp4_mp_hd_720p_aac,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_avc_mp4_mp_hd_720p_aac,
        .audio_rule = &_dlna_audio_rule_avc_mp4_mp_hd_720p_aac,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mp4_mp_hd_720p_aac,
        .dlna_mime = &_dlna_mime_video,
        .video_rules = _dlna_video_rule_avc_mp4_mp_hd_1080i_aac,
        .audio_rule = &_dlna_audio_rule_avc_mp4_mp_hd_1080i_aac,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mkv_mp_hd_aac_mult5,
        .dlna_mime = &_dlna_mime_video_matroska,
        .video_rules = _dlna_video_avc_mkv_hp_hd,
        .audio_rule = &_dlna_audio_rule_avc_mkv_mp_hd_aac_mult5,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mkv_hp_hd_aac_mult5, // same rules as mp
        .dlna_mime = &_dlna_mime_video_matroska,
        .video_rules = _dlna_video_avc_mkv_hp_hd,
        .audio_rule = &_dlna_audio_rule_avc_mkv_mp_hd_aac_mult5,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mkv_mp_hd_ac3,
        .dlna_mime = &_dlna_mime_video_matroska,
        .video_rules = _dlna_video_avc_mkv_hp_hd,
        .audio_rule = &_dlna_audio_rule_avc_mkv_mp_hd_ac3,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mkv_hp_hd_ac3, // same rules as mp
        .dlna_mime = &_dlna_mime_video_matroska,
        .video_rules = _dlna_video_avc_mkv_hp_hd,
        .audio_rule = &_dlna_audio_rule_avc_mkv_mp_hd_ac3,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mkv_mp_hd_mpeg1_l3,
        .dlna_mime = &_dlna_mime_video_matroska,
        .video_rules = _dlna_video_avc_mkv_hp_hd,
        .audio_rule = &_dlna_audio_rule_avc_mkv_mp_hd_mpeg1_l3,
    },
    {
        .dlna_profile = &_dlna_profile_avc_mkv_hp_hd_mpeg1_l3, // same rules as mp
        .dlna_mime = &_dlna_mime_video_matroska,
        .video_rules = _dlna_video_avc_mkv_hp_hd,
        .audio_rule = &_dlna_audio_rule_avc_mkv_mp_hd_mpeg1_l3,
    },
    {NULL},
};

#define DLNA_AUDIO_RULE_AAC_ISO                                         \
    .codec = &_codec_audio_mpeg4aac_lc,                                 \
        .channels = DLNA_BITRATE(1, 2),                                 \
        .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000,             \
                                 22050, 24000, 32000, 44100, 48000),    \
        .bitrate = DLNA_BITRATE(0, 576000)                              \

static const struct lms_dlna_audio_rule _dlna_audio_rule_aac_iso = {
    DLNA_AUDIO_RULE_AAC_ISO,
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_aac_iso_3gp = {
    DLNA_AUDIO_RULE_AAC_ISO,
};

#define DLNA_AUDIO_RULE_AAC_ISO_320                                     \
    .codec = &_codec_audio_mpeg4aac_lc,                                 \
        .channels = DLNA_BITRATE(1, 2),                                 \
        .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000,             \
                                 22050, 24000, 32000, 44100, 48000),    \
        .bitrate = DLNA_BITRATE(0, 320000)                              \

static const struct lms_dlna_audio_rule _dlna_audio_rule_aac_iso_320 = {
    DLNA_AUDIO_RULE_AAC_ISO_320,
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_aac_iso_320_3gp = {
    DLNA_AUDIO_RULE_AAC_ISO_320,
};

#define DLNA_AUDIO_RULE_AAC_MULT5_ISO                                   \
    .codec = &_codec_audio_mpeg4aac_lc,                                 \
        .channels = DLNA_BITRATE(1, 6),                                 \
        .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000,             \
                                 22050, 24000, 32000, 44100, 48000),    \
        .bitrate = DLNA_BITRATE(0, 1440000)                             \

static const struct lms_dlna_audio_rule _dlna_audio_rule_aac_mult5_iso = {
    DLNA_AUDIO_RULE_AAC_MULT5_ISO,
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_aac_mult5_iso_3gp = {
    DLNA_AUDIO_RULE_AAC_MULT5_ISO,
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_ac3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(64000, 64000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_eac3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
    .bitrate = DLNA_BITRATE( 32000, 6144000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_amr_3gpp = {
    .codec = &_codec_audio_amr,
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_amr = {
    .codec = &_codec_audio_amr,
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_amr_wbplus = {
    .codec = &_codec_audio_amr_wb,
    .rates = DLNA_AUDIO_RATE(8000, 16000, 24000, 32000, 48000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_amr_wbplus_3gp = {
    .codec = &_codec_audio_amr_wb,
    .rates = DLNA_AUDIO_RATE(8000, 16000, 24000, 32000, 48000),
};

const struct lms_dlna_audio_profile _dlna_mp4_audio_rules[] = {
    {
        .dlna_profile = &_dlna_profile_aac_iso,
        .dlna_mime = &_dlna_mime_audio,
        .audio_rule = &_dlna_audio_rule_aac_iso,
    },
    {
        .dlna_profile = &_dlna_profile_aac_iso_320,
        .dlna_mime = &_dlna_mime_audio,
        .audio_rule = &_dlna_audio_rule_aac_iso_320,
    },
    {
        .dlna_profile = &_dlna_profile_aac_mult5_iso,
        .dlna_mime = &_dlna_mime_audio,
        .audio_rule = &_dlna_audio_rule_aac_mult5_iso,
    },
    {
        .dlna_profile = &_dlna_profile_ac3,
        .dlna_mime = &_dlna_mime_audio_dolby,
        .audio_rule = &_dlna_audio_rule_ac3,
    },
    {
        .dlna_profile = &_dlna_profile_eac3,
        .dlna_mime = &_dlna_mime_audio_eac3,
        .audio_rule = &_dlna_audio_rule_eac3,
    },
    {
        .dlna_profile = &_dlna_profile_amr_3gpp,
        .dlna_mime = &_dlna_mime_audio,
        .audio_rule = &_dlna_audio_rule_amr,
    },
    {
        .dlna_profile = &_dlna_profile_amr_wbplus,
        .dlna_mime = &_dlna_mime_audio,
        .audio_rule = &_dlna_audio_rule_amr_wbplus,
    },
    {NULL}
};

const struct lms_dlna_audio_profile _dlna_3gp_audio_rules[] = {
    {
        .dlna_profile = &_dlna_profile_aac_iso,
        .dlna_mime = &_dlna_mime_audio_3gp,
        .audio_rule = &_dlna_audio_rule_aac_iso_3gp,
    },
    {
        .dlna_profile = &_dlna_profile_aac_iso_320,
        .dlna_mime = &_dlna_mime_audio_3gp,
        .audio_rule = &_dlna_audio_rule_aac_iso_320_3gp,
    },
    {
        .dlna_profile = &_dlna_profile_aac_mult5_iso,
        .dlna_mime = &_dlna_mime_audio_3gp,
        .audio_rule = &_dlna_audio_rule_aac_mult5_iso_3gp,
    },
    {
        .dlna_profile = &_dlna_profile_amr_3gpp,
        .dlna_mime = &_dlna_mime_audio_3gp,
        .audio_rule = &_dlna_audio_rule_amr_3gpp,
    },
    {
        .dlna_profile = &_dlna_profile_amr_wbplus,
        .dlna_mime = &_dlna_mime_audio_3gp,
        .audio_rule = &_dlna_audio_rule_amr_wbplus_3gp,
    },
    {NULL}
};

/** asf */
DECL_STR(_codec_audio_wmavpro, "wmavpro");

DECL_STR(_dlna_wma_base, "WMABASE");
DECL_STR(_dlna_wma_full, "WMAFULL");
DECL_STR(_dlna_wma_pro, "WMAPRO");
DECL_STR(_dlna_wma_mime, "audio/x-ms-wma");

static const struct lms_dlna_audio_rule _dlna_audio_rule_wma_base = {
    .rate_range = DLNA_BITRATE(0, 48000),
    .bitrate = DLNA_BITRATE(0, 192999),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_wma_full = {
    .rate_range = DLNA_BITRATE(0, 48000),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_wma_pro = {
    .codec = &_codec_audio_wmavpro,
    .rate_range = DLNA_BITRATE(0, 96000),
    .channels = DLNA_BITRATE(1, 8),
    .bitrate = DLNA_BITRATE(0, 1500000),
};

const struct lms_dlna_audio_profile _dlna_wma_rules[] = {
    {
        .dlna_profile = &_dlna_wma_base,
        .dlna_mime = &_dlna_wma_mime,
        .audio_rule = &_dlna_audio_rule_wma_base,
    },
    {
        .dlna_profile = &_dlna_wma_full,
        .dlna_mime = &_dlna_wma_mime,
        .audio_rule = &_dlna_audio_rule_wma_full,
    },
    {
        .dlna_profile = &_dlna_wma_pro,
        .dlna_mime = &_dlna_wma_mime,
        .audio_rule = &_dlna_audio_rule_wma_pro,
    },
    {NULL}
};

/** wma */
DECL_STR(_dlna_profile_wave_lpcm, "LPCM");

DECL_STR(_dlna_mime_44_mono, "audio/L16;rate=44100;channels=1");
DECL_STR(_dlna_mime_44_stereo, "audio/L16;rate=44100;channels=2");
DECL_STR(_dlna_mime_48_mono, "audio/L16;rate=48000;channels=1");
DECL_STR(_dlna_mime_48_stereo, "audio/L16;rate=48000;channels=2");

static const struct lms_dlna_audio_rule _dlna_audio_rule_wave_lpcm_44_mono = {
    .rates = DLNA_AUDIO_RATE(44100),
    .channels = DLNA_BITRATE(1, 1),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_wave_lpcm_44_stereo = {
    .rates = DLNA_AUDIO_RATE(44100),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_wave_lpcm_48_mono = {
    .rates = DLNA_AUDIO_RATE(48000),
    .channels = DLNA_BITRATE(1, 1),
};

static const struct lms_dlna_audio_rule _dlna_audio_rule_wave_lpcm_48_stereo = {
    .rates = DLNA_AUDIO_RATE(48000),
    .channels = DLNA_BITRATE(1, 2),
};

const struct lms_dlna_audio_profile _dlna_wave_rules[] = {
    {
        .dlna_profile = &_dlna_profile_wave_lpcm,
        .dlna_mime = &_dlna_mime_44_mono,
        .audio_rule = &_dlna_audio_rule_wave_lpcm_44_mono,
    },
    {
        .dlna_profile = &_dlna_profile_wave_lpcm,
        .dlna_mime = &_dlna_mime_44_stereo,
        .audio_rule = &_dlna_audio_rule_wave_lpcm_44_stereo,
    },
    {
        .dlna_profile = &_dlna_profile_wave_lpcm,
        .dlna_mime = &_dlna_mime_48_mono,
        .audio_rule = &_dlna_audio_rule_wave_lpcm_48_mono,
    },
    {
        .dlna_profile = &_dlna_profile_wave_lpcm,
        .dlna_mime = &_dlna_mime_48_stereo,
        .audio_rule = &_dlna_audio_rule_wave_lpcm_48_stereo,
    },
    {NULL}
};

/** jpeg */
DECL_STR(_dlna_jpeg_mime, "image/jpeg");

DECL_STR(_dlna_jpeg_sm_ico, "JPEG_SM_ICO");
DECL_STR(_dlna_jpeg_lrg_ico, "JPEG_LRG_ICO");
DECL_STR(_dlna_jpeg_tn, "JPEG_TN");
DECL_STR(_dlna_jpeg_sm, "JPEG_SM");
DECL_STR(_dlna_jpeg_med, "JPEG_MED");
DECL_STR(_dlna_jpeg_lrg, "JPEG_LRG");

static const struct lms_dlna_image_rule _dlna_image_rule_jpeg_sm_ico = {
    .width = DLNA_BITRATE(48, 48),
    .height = DLNA_BITRATE(48, 48),
};

static const struct lms_dlna_image_rule _dlna_image_rule_jpeg_lrg_ico = {
    .width = DLNA_BITRATE(0, 120),
    .height = DLNA_BITRATE(0, 120),
};

static const struct lms_dlna_image_rule _dlna_image_rule_jpeg_tn = {
    .width = DLNA_BITRATE(0, 160),
    .height = DLNA_BITRATE(0, 160),
};

static const struct lms_dlna_image_rule _dlna_image_rule_jpeg_sm = {
    .width = DLNA_BITRATE(0, 640),
    .height = DLNA_BITRATE(0, 480),
};

static const struct lms_dlna_image_rule _dlna_image_rule_jpeg_med = {
    .width = DLNA_BITRATE(0, 1024),
    .height = DLNA_BITRATE(0, 768),
};

static const struct lms_dlna_image_rule _dlna_image_rule_jpeg_lrg = {
    .width = DLNA_BITRATE(0, 4096),
    .height = DLNA_BITRATE(0, 4096),
};

const struct lms_dlna_image_profile _dlna_jpeg_rules[] = {
    {
        .dlna_profile = &_dlna_jpeg_sm_ico,
        .dlna_mime = &_dlna_jpeg_mime,
        .image_rule = &_dlna_image_rule_jpeg_sm_ico,
    },
    {
        .dlna_profile = &_dlna_jpeg_lrg_ico,
        .dlna_mime = &_dlna_jpeg_mime,
        .image_rule = &_dlna_image_rule_jpeg_lrg_ico,
    },
    {
        .dlna_profile = &_dlna_jpeg_tn,
        .dlna_mime = &_dlna_jpeg_mime,
        .image_rule = &_dlna_image_rule_jpeg_tn,
    },
    {
        .dlna_profile = &_dlna_jpeg_sm,
        .dlna_mime = &_dlna_jpeg_mime,
        .image_rule = &_dlna_image_rule_jpeg_sm,
    },
    {
        .dlna_profile = &_dlna_jpeg_med,
        .dlna_mime = &_dlna_jpeg_mime,
        .image_rule = &_dlna_image_rule_jpeg_med,
    },
    {
        .dlna_profile = &_dlna_jpeg_lrg,
        .dlna_mime = &_dlna_jpeg_mime,
        .image_rule = &_dlna_image_rule_jpeg_lrg,
    },
    {NULL}
};

/** png */

DECL_STR(_dlna_png_mime, "image/png");

DECL_STR(_dlna_png_sm_ico, "PNG_SM_ICO");
DECL_STR(_dlna_png_lrg_ico, "PNG_LRG_ICO");
DECL_STR(_dlna_png_tn, "PNG_TN");
DECL_STR(_dlna_png_lrg, "PNG_LRG");

/** using the same rules as jpeg but in a smaller subset */
const struct lms_dlna_image_profile _dlna_png_rules[] = {
    {
        .dlna_profile = &_dlna_jpeg_sm_ico,
        .dlna_mime = &_dlna_jpeg_mime,
        .image_rule = &_dlna_image_rule_jpeg_sm_ico,
    },
    {
        .dlna_profile = &_dlna_jpeg_lrg_ico,
        .dlna_mime = &_dlna_jpeg_mime,
        .image_rule = &_dlna_image_rule_jpeg_lrg_ico,
    },
    {
        .dlna_profile = &_dlna_jpeg_tn,
        .dlna_mime = &_dlna_jpeg_mime,
        .image_rule = &_dlna_image_rule_jpeg_tn,
    },
    {
        .dlna_profile = &_dlna_jpeg_lrg,
        .dlna_mime = &_dlna_jpeg_mime,
        .image_rule = &_dlna_image_rule_jpeg_lrg,
    },
    {NULL}
};
