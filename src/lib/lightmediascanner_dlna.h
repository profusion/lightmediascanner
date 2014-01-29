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

#include <lightmediascanner_db.h>
#include <stdio.h>
#include <math.h>

#ifndef _LIGHTMEDIASCANNER_DLNA_H_
#define _LIGHTMEDIASCANNER_DLNA_H_

#define MAX_AUDIO_MPEG_VERSIONS 2
#define MAX_AUDIO_RATES 10
#define MAX_AUDIO_LEVELS 5
#define MAX_VIDEO_RULE_LEVEL 13
#define MAX_VIDEO_RULE_PROFILE 3
#define MAX_VIDEO_RULE_FRAMERATE 7
#define MAX_VIDEO_PIXEL_ASPECT 3

#define DLNA_VIDEO_RES(_width, _height)                                 \
    &(struct dlna_video_res) {.width = _width, .height = _height}       \

#define DLNA_VIDEO_RES_RANGE(_wmin, _wmax, _hmin, _hmax)                \
    &(struct dlna_video_res_range) {.width_min = _wmin,                 \
                                         .width_max = _wmax, .height_min = _hmin, \
                                         .height_max = _hmax}           \

#define DLNA_BITRATE(_min, _max)                        \
    &(struct dlna_bitrate) {.min = _min, .max = _max}   \

#define DLNA_LEVEL(_val...)				\
    &(struct dlna_level) {.levels = {_val, NULL}}       \

#define DLNA_PROFILE(_val...)                           \
    &(struct dlna_profile) {.profiles = {_val, NULL}}   \

#define DLNA_AUDIO_RATE(_val...)				\
    &(struct dlna_audio_rate) {.rates = {_val, UINT32_MAX}}	\

#define DLNA_VIDEO_PIXEL_ASPECT(_val...)				\
    &(struct dlna_video_pixel_aspect) {					\
                                       .pixel_aspect_ratio = {_val, NULL}} \

#define DLNA_VIDEO_FRAMERATE(_val...)					\
    &(struct dlna_video_framerate) {.framerate = {_val, NAN}}           \

#define DLNA_VIDEO_PACKETSIZE(_packet_size)				\
    &(struct dlna_video_packet_size) {.packet_size = _packet_size}	\

#define DLNA_VIDEO_FRAMERATE_RANGE(_min, _max)                          \
    &(struct dlna_video_framerate_range) {.min = _min, .max = _max}     \

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

struct dlna_video_framerate_range {
    double min;
    double max;
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

struct lms_dlna_video_rule {
    const struct dlna_video_res *res;
    const struct dlna_video_res_range *res_range;
    const struct dlna_bitrate *bitrate;
    const struct dlna_profile *profiles;
    const struct dlna_level *levels;
    const struct dlna_video_framerate *framerate;
    const struct dlna_video_pixel_aspect *pixel_aspect;
    struct dlna_video_framerate_range *framerate_range;
};

struct dlna_audio_rate {
    const unsigned int rates[MAX_AUDIO_RATES];
};

struct lms_dlna_audio_rule {
    const struct lms_string_size *codec;
    const struct dlna_audio_rate *rates;
    const struct dlna_level *levels;
    const struct dlna_bitrate *channels;
    const struct dlna_bitrate *bitrate;
    const struct dlna_bitrate *rate_range;
};

struct dlna_video_packet_size {
    const int64_t packet_size;
};

struct lms_dlna_video_profile {
    const struct lms_string_size *dlna_profile;
    const struct lms_string_size *dlna_mime;
    const struct lms_dlna_video_rule *video_rules;
    const struct lms_dlna_audio_rule *audio_rule;
    const struct dlna_video_packet_size *packet_size;
};

struct lms_dlna_image_rule {
    const struct dlna_bitrate *width;
    const struct dlna_bitrate *height;
};

struct lms_dlna_audio_profile {
    const struct lms_string_size *dlna_profile;
    const struct lms_string_size *dlna_mime;
    const struct lms_dlna_audio_rule *audio_rule;
};

struct lms_dlna_image_profile {
    const struct lms_string_size *dlna_profile;
    const struct lms_string_size *dlna_mime;
    const struct lms_dlna_image_rule *image_rule;
};

const struct lms_dlna_video_profile *lms_dlna_get_video_profile(struct lms_video_info *info);
const struct lms_dlna_audio_profile *lms_dlna_get_audio_profile(struct lms_audio_info *info);
const struct lms_dlna_image_profile *lms_dlna_get_image_profile(struct lms_image_info *info);

extern const struct lms_string_size _container_mp3;
extern const struct lms_string_size _container_mpeg;
extern const struct lms_string_size _container_mpegts;

extern const struct lms_dlna_image_profile _dlna_jpeg_rules[];
extern const struct lms_dlna_image_profile _dlna_png_rules[];

extern const struct lms_dlna_audio_profile _dlna_mp3_rules[];
extern const struct lms_dlna_audio_profile _dlna_mp4_audio_rules[];
extern const struct lms_dlna_audio_profile _dlna_3gp_audio_rules[];
extern const struct lms_dlna_audio_profile _dlna_wma_rules[];
extern const struct lms_dlna_audio_profile _dlna_wave_rules[];

extern const struct lms_dlna_video_profile _dlna_mpeg_rules[];
extern const struct lms_dlna_video_profile _dlna_mpegts_rules[];
extern const struct lms_dlna_video_profile _dlna_mp4_video_rules[];

#endif /* _LIGHTMEDIASCANNER_DLNA_H_ */
