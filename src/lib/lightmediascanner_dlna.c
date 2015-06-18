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
#include <lightmediascanner_dlna.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dlna_video_container_rule {
    const struct lms_string_size *container;
    const struct lms_dlna_video_profile *rules;
};

struct dlna_audio_container_rule {
    const struct lms_string_size *container;
    const struct lms_dlna_audio_profile *rules;
};

struct dlna_image_container_rule {
    const struct lms_string_size *container;
    const struct lms_dlna_image_profile *rules;
};

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

DECL_STR(_container_mp3, "mp3");
DECL_STR(_container_mp4, "mp4");
DECL_STR(_container_3gp, "3gp");
DECL_STR(_container_asf, "asf");
DECL_STR(_container_wma2, "wma2");
DECL_STR(_container_jpeg, "jpeg");
DECL_STR(_container_png, "png");
DECL_STR(_container_wave, "wave");
DECL_STR(_container_mpeg, "mpeg");
DECL_STR(_container_mpegts, "mpegts");

static const struct dlna_video_container_rule _video_container_rules[] = {
    {
        .container = &_container_mpeg,
        .rules = _dlna_mpeg_rules,
    },
    {
        .container = &_container_mpegts,
        .rules = _dlna_mpegts_rules,
    },
    {
        .container = &_container_mp4,
        .rules = _dlna_mp4_video_rules,
    },
};

static const struct dlna_audio_container_rule _audio_container_rules[] = {
    {
        .container = &_container_mp3,
        .rules = _dlna_mp3_rules,
    },
    {
        .container = &_container_3gp,
        .rules = _dlna_3gp_audio_rules,
    },
    {
        .container = &_container_mp4,
        .rules = _dlna_mp4_audio_rules,
    },
    {
        .container = &_container_asf,
        .rules = _dlna_wma_rules,
    },
    {
        .container = &_container_wma2,
        .rules = _dlna_wma_rules,
    },
    {
        .container = &_container_wave,
        .rules = _dlna_wave_rules,
    },
};

static const struct dlna_image_container_rule _image_container_rules[] = {
    {
        .container = &_container_jpeg,
        .rules = _dlna_jpeg_rules,
    },
    {
        .container = &_container_png,
        .rules = _dlna_png_rules,
    },
};

static bool
_uint_vector_has_value(const unsigned int *list, const unsigned int wanted)
{
    const unsigned int *itr;

    for (itr = list; *itr != UINT32_MAX; itr++)
        if (*itr == wanted) return true;

    return false;
}

static bool
_double_vector_has_value(const double *list, const double wanted, const double accepted_error)
{
    const double *itr;

    for (itr = list; !isnan(*itr); itr++)
        if (fabs(*itr - wanted) < accepted_error) return true;

    return false;
}

static bool
_string_vector_has_value(const char **list, const char *wanted)
{
    const char **itr;

    for (itr = list; *itr != NULL; itr++)
        if (!strcmp(*itr, wanted)) return true;

    return false;
}

static inline bool
_dlna_audio_rule_match_stream(const struct lms_dlna_audio_rule *rule,
                              const struct lms_stream *audio_stream) {

    if (audio_stream) {
        if (rule->bitrate &&
            (audio_stream->audio.bitrate < rule->bitrate->min ||
             audio_stream->audio.bitrate > rule->bitrate->max)) {
            return false;
        }
        if (rule->rates &&
            !_uint_vector_has_value(rule->rates->rates,
                                    audio_stream->audio.sampling_rate))
            return false;

        if (rule->channels &&
            (audio_stream->audio.channels < rule->channels->min ||
             audio_stream->audio.channels > rule->channels->max))
            return false;
    }

    if (audio_stream && rule->codec &&
        strcmp(audio_stream->codec.str, rule->codec->str))
        return false;

    return true;
}

static inline bool
_video_rule_match_stream(const struct lms_dlna_video_rule *rule,
                         const char *profile, const char *level,
                         const struct lms_stream *video) {
    if (rule->res && 
        (video->video.width != rule->res->width &&
         video->video.height != rule->res->height))
        return false;

    if (rule->res_range &&
        !(video->video.width >= rule->res_range->width_min &&
          video->video.width <= rule->res_range->width_max &&
          video->video.height >= rule->res_range->height_min &&
          video->video.height <= rule->res_range->height_max))
        return false;

    if (rule->framerate &&
        !_double_vector_has_value(rule->framerate->framerate,
                                  video->video.framerate, 0.1))
        return false;

    if (rule->pixel_aspect && !_string_vector_has_value
        ((const char **)rule->pixel_aspect->pixel_aspect_ratio,
         video->video.aspect_ratio.str))
        return false;

    if (rule->bitrate &&
        !(video->video.bitrate >= rule->bitrate->min &&
          video->video.bitrate <= rule->bitrate->max))
        return false;

    if (rule->levels && level &&
        !_string_vector_has_value
        ((const char **)rule->levels->levels, level))
        return false;

    if (rule->profiles && profile &&
        !_string_vector_has_value
        ((const char **)rule->profiles->profiles, profile))
        return false;

    return true;
}

static bool
_video_rule_is_sentinel(const struct lms_dlna_video_rule *video_rule)
{
    return (!video_rule->res && !video_rule->bitrate && !video_rule->levels);
}

static const struct lms_dlna_video_profile *
_match_video_profile(const struct lms_dlna_video_profile *video_rules,
                     const struct lms_stream *video,
                     const struct lms_stream *audio,
                     const int64_t packet_size) {
    int i;
    const struct lms_dlna_video_profile *curr;
    char *tmp, *p;
    const char *profile = NULL, *level = NULL;

    if (!video)
        return NULL;

    tmp = strdupa(video->codec.str);
    p = strstr(tmp, "-p");
    if (p) {
        p[0] = '\0';
        p += 2;
        profile = p;
    } else {
        p = tmp;
    }

    p = strstr(p, "-l");
    if (p) {
        p[0] = '\0';
        p += 2;
        level = p;
    }

    curr = video_rules;
    for (i = 0; curr->dlna_profile != NULL; i++, curr = video_rules + i) {
        const struct lms_dlna_video_rule *video_rule;
        const struct lms_dlna_audio_rule *audio_rule;

        audio_rule = curr->audio_rule;

        if (curr->packet_size &&
            curr->packet_size->packet_size != packet_size)
            continue;

        if (!_dlna_audio_rule_match_stream(audio_rule, audio))
            continue;

        for (video_rule = curr->video_rules;
             !_video_rule_is_sentinel(video_rule); video_rule++) {

            if (_video_rule_match_stream(video_rule, profile, level, video))
                return curr;
        }
    }

    return NULL;
}

static const struct lms_dlna_video_profile *
_get_video_rules(const struct lms_video_info *info) {
    int i, length;

    if (!info->container.str) return NULL;

    length = sizeof(_video_container_rules) /
        sizeof(struct dlna_video_container_rule);

    for (i = 0; i < length; i++) {
        const struct dlna_video_container_rule *curr;

        curr = _video_container_rules + i;
        if (!strcmp(curr->container->str, info->container.str))
            return curr->rules;
    }

    return NULL;
}

static const struct lms_dlna_audio_profile *
_get_audio_rules(const struct lms_audio_info *info) {
    int i, length;

    if (!info->container.str) return NULL;

    length = sizeof(_audio_container_rules) /
        sizeof(struct dlna_audio_container_rule);

    for (i = 0; i < length; i++) {
        const struct dlna_audio_container_rule *curr;

        curr = _audio_container_rules + i;
        if (!strcmp(curr->container->str, info->container.str))
            return curr->rules;
    }

    return NULL;
}

static const struct lms_dlna_image_profile *
_get_image_rules(const struct lms_image_info *info) {
    int i, length;

    if (!info->container.str) return NULL;

    length = sizeof(_image_container_rules) /
        sizeof(struct dlna_image_container_rule);

    for (i = 0; i < length; i++) {
        const struct dlna_image_container_rule *curr;

        curr = _image_container_rules + i;
        if (!strcmp(curr->container->str, info->container.str))
            return curr->rules;
    }

    return NULL;
}

const struct lms_dlna_video_profile *
lms_dlna_get_video_profile(struct lms_video_info *info) {
    const struct lms_dlna_video_profile *rules;
    const struct lms_stream *s, *audio_stream, *video_stream;

    audio_stream = video_stream = NULL;

    rules = _get_video_rules(info);
    if (!rules) return NULL;

    for (s = info->streams; s; s = s->next) {
        if (s->type == LMS_STREAM_TYPE_VIDEO) {
            video_stream = s;
            if (audio_stream) break;
        } else if (s->type == LMS_STREAM_TYPE_AUDIO) {
            audio_stream = s;
            if (video_stream) break;
        }
    }

    return _match_video_profile(rules, video_stream, audio_stream,
                                info->packet_size);
}

static bool
_audio_rule_is_sentinel(const struct lms_dlna_audio_profile *audio_rule)
{
    return (!audio_rule->dlna_profile && !audio_rule->dlna_mime &&
            !audio_rule->audio_rule);
}

const struct lms_dlna_audio_profile *
lms_dlna_get_audio_profile(struct lms_audio_info *info) {
    const struct lms_dlna_audio_profile *rules = _get_audio_rules(info);
    const struct lms_dlna_audio_profile *profile;

    if (!rules) return NULL;

    for (profile = rules; !_audio_rule_is_sentinel(profile); profile++) {
        const struct lms_dlna_audio_rule *rule = profile->audio_rule;

        if (rule->bitrate && info->bitrate &&
            (info->bitrate < rule->bitrate->min ||
             info->bitrate > rule->bitrate->max))
            continue;

        if (rule->rates &&
            !_uint_vector_has_value(rule->rates->rates, info->sampling_rate))
            continue;

        if (rule->rate_range &&
            (info->sampling_rate < rule->rate_range->min ||
             info->sampling_rate > rule->rate_range->max))
            continue;

        if (rule->channels &&
            (info->channels < rule->channels->min ||
             info->channels > rule->channels->max))
            continue;

        if (rule->codec && strcmp(rule->codec->str, info->codec.str))
            continue;

        return profile;
    }

    return NULL;
}

static bool
_image_rule_is_sentinel(const struct lms_dlna_image_profile *rule)
{
    return (!rule->dlna_profile && !rule->dlna_mime && !rule->image_rule);
}

const struct lms_dlna_image_profile *
lms_dlna_get_image_profile(struct lms_image_info *info) {
    const struct lms_dlna_image_profile *rules = _get_image_rules(info);
    const struct lms_dlna_image_profile *profile;

    if (!rules) return NULL;

    for (profile = rules; !_image_rule_is_sentinel(profile); profile++) {
        const struct lms_dlna_image_rule *rule = profile->image_rule;

        if (rule->width && info->width &&
            (info->width < rule->width->min ||
             info->width > rule->width->max))
            continue;

        if (rule->height && info->height &&
            (info->height < rule->height->min ||
             info->height > rule->height->max))
            continue;

        return profile;
    }

    return NULL;
}
