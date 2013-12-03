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
 * mp4 file parser.
 */

static const char PV[] = PACKAGE_VERSION; /* mp4.h screws PACKAGE_VERSION */

#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>

#include <mp4v2/mp4v2.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

struct mp4_info {
    struct lms_string_size title;
    struct lms_string_size artist;
    struct lms_string_size album;
    struct lms_string_size genre;
    uint16_t trackno;
    uint64_t length;
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
    lms_db_video_t *video_db;
};

#define DECL_STR(cname, str)                                            \
    static const struct lms_string_size cname = LMS_STATIC_STRING_SIZE(str)

DECL_STR(_container_mp4, "mp4");
DECL_STR(_container_3gp, "3gp");

DECL_STR(_codec_audio_mpeg4aac_main, "mpeg4aac-main");
DECL_STR(_codec_audio_mpeg4aac_lc, "mpeg4aac-lc");
DECL_STR(_codec_audio_mpeg4aac_ssr, "mpeg4aac-ssr");
DECL_STR(_codec_audio_mpeg4aac_ltp, "mpeg4aac-ltp");
DECL_STR(_codec_audio_mpeg4aac_he, "mpeg4aac-he");
DECL_STR(_codec_audio_mpeg4aac_scalable, "mpeg4aac-scalable");
DECL_STR(_codec_audio_mpeg4_twinvq, "mpeg4twinvq");
DECL_STR(_codec_audio_mpeg4_celp, "mpeg4celp");
DECL_STR(_codec_audio_mpeg4_hvxc, "mpeg4hvxc");
DECL_STR(_codec_audio_mpeg4_tssi, "mpeg4ttsi");
DECL_STR(_codec_audio_mpeg4_main_synthetic,"mpeg4main-synthetic");
DECL_STR(_codec_audio_mpeg4_wavetable_syn, "mpeg4wavetable-syn");
DECL_STR(_codec_audio_mpeg4_general_midi, "mpeg4general-midi");
DECL_STR(_codec_audio_mpeg4_algo_syn_and_audio_fx, "mpeg4algo-syn-and-audio-fx");
DECL_STR(_codec_audio_mpeg4_er_aac_lc, "mpeg4er-aac-lc");
DECL_STR(_codec_audio_mpeg4_er_aac_ltp, "mpeg4er-aac-ltp");
DECL_STR(_codec_audio_mpeg4_er_aac_scalable, "mpeg4er-aac-scalable");
DECL_STR(_codec_audio_mpeg4_er_twinvq, "mpeg4er-twinvq");
DECL_STR(_codec_audio_mpeg4_er_bsac, "mpeg4er-bsac");
DECL_STR(_codec_audio_mpeg4_er_acc_ld, "mpeg4er-acc-ld");
DECL_STR(_codec_audio_mpeg4_er_celp, "mpeg4er-celp");
DECL_STR(_codec_audio_mpeg4_er_hvxc, "mpeg4er-hvxc");
DECL_STR(_codec_audio_mpeg4_er_hiln, "mpeg4er-hiln");
DECL_STR(_codec_audio_mpeg4_er_parametric, "mpeg4er-parametric");
DECL_STR(_codec_audio_mpeg4_ssc, "mpeg4ssc");
DECL_STR(_codec_audio_mpeg4_ps, "mpeg4ps");
DECL_STR(_codec_audio_mpeg4_mpeg_surround, "mpeg4mpeg-surround");
DECL_STR(_codec_audio_mpeg4_layer1, "mpeg4layer1");
DECL_STR(_codec_audio_mpeg4_layer2, "mpeg4layer2");
DECL_STR(_codec_audio_mpeg4_layer3, "mpeg4layer3");
DECL_STR(_codec_audio_mpeg4_dst, "mpeg4dst");
DECL_STR(_codec_audio_mpeg4_audio_lossless, "mpeg4audio-lossless");
DECL_STR(_codec_audio_mpeg4_sls, "mpeg4sls");
DECL_STR(_codec_audio_mpeg4_sls_non_core, "mpeg4sls-non-core");

DECL_STR(_codec_audio_mpeg2aac_main, "mpeg2aac-main");
DECL_STR(_codec_audio_mpeg2aac_lc, "mpeg2aac-lc");
DECL_STR(_codec_audio_mpeg2aac_ssr, "mpeg2aac-ssr");
DECL_STR(_codec_audio_mpeg2audio, "mpeg2audio");
DECL_STR(_codec_audio_mpeg1audio, "mpeg1audio");
DECL_STR(_codec_audio_pcm16le, "pcm16le");
DECL_STR(_codec_audio_vorbis, "vorbis");
DECL_STR(_codec_audio_alaw, "alaw");
DECL_STR(_codec_audio_ulaw, "ulaw");
DECL_STR(_codec_audio_g723, "g723");
DECL_STR(_codec_audio_pcm16be, "pcm16be");

DECL_STR(_codec_video_mpeg4_simple_l1, "mpeg4-simple-l1");
DECL_STR(_codec_video_mpeg4_simple_l2, "mpeg4-simple-l2");
DECL_STR(_codec_video_mpeg4_simple_l3, "mpeg4-simple-l3");
DECL_STR(_codec_video_mpeg4_simple_l0, "mpeg4-simple-l0");
DECL_STR(_codec_video_mpeg4_simple_scalable_l1, "mpeg4-simple-scalable-l1");
DECL_STR(_codec_video_mpeg4_simple_scalable_l2, "mpeg4-simple-scalable-l2");
DECL_STR(_codec_video_mpeg4_core_l1, "mpeg4-core-l1");
DECL_STR(_codec_video_mpeg4_core_l2, "mpeg4-core-l2");
DECL_STR(_codec_video_mpeg4_main_l2, "mpeg4-main-l2");
DECL_STR(_codec_video_mpeg4_main_l3, "mpeg4-main-l3");
DECL_STR(_codec_video_mpeg4_main_l4, "mpeg4-main-l4");
DECL_STR(_codec_video_mpeg4_nbit_l2, "mpeg4-nbit-l2");
DECL_STR(_codec_video_mpeg4_scalable_texture_l1, "mpeg4-scalable-texture-l1");
DECL_STR(_codec_video_mpeg4_simple_face_anim_l1, "mpeg4-simple-face-anim-l1");
DECL_STR(_codec_video_mpeg4_simple_face_anim_l2, "mpeg4-simple-face-anim-l2");
DECL_STR(_codec_video_mpeg4_simple_fba_l1, "mpeg4-simple-fba-l1");
DECL_STR(_codec_video_mpeg4_simple_fba_l2, "mpeg4-simple-fba-l2");
DECL_STR(_codec_video_mpeg4_basic_anim_text_l1, "mpeg4-basic-anim-text-l1");
DECL_STR(_codec_video_mpeg4_basic_anim_text_l2, "mpeg4-basic-anim-text-l2");
DECL_STR(_codec_video_mpeg4_hybrid_l1, "mpeg4-hybrid-l1");
DECL_STR(_codec_video_mpeg4_hybrid_l2, "mpeg4-hybrid-l2");
DECL_STR(_codec_video_mpeg4_adv_rt_simple_l1, "mpeg4-adv-rt-simple-l1");
DECL_STR(_codec_video_mpeg4_adv_rt_simple_l2, "mpeg4-adv-rt-simple-l2");
DECL_STR(_codec_video_mpeg4_adv_rt_simple_l3, "mpeg4-adv-rt-simple-l3");
DECL_STR(_codec_video_mpeg4_adv_rt_simple_l4, "mpeg4-adv-rt-simple-l4");
DECL_STR(_codec_video_mpeg4_core_scalable_l1, "mpeg4-core-scalable-l1");
DECL_STR(_codec_video_mpeg4_core_scalable_l2, "mpeg4-core-scalable-l2");
DECL_STR(_codec_video_mpeg4_core_scalable_l3, "mpeg4-core-scalable-l3");
DECL_STR(_codec_video_mpeg4_adv_coding_efficiency_l1, "mpeg4-adv-coding-efficiency-l1");
DECL_STR(_codec_video_mpeg4_adv_coding_efficiency_l2, "mpeg4-adv-coding-efficiency-l2");
DECL_STR(_codec_video_mpeg4_adv_coding_efficiency_l3, "mpeg4-adv-coding-efficiency-l3");
DECL_STR(_codec_video_mpeg4_adv_coding_efficiency_l4, "mpeg4-adv-coding-efficiency-l4");
DECL_STR(_codec_video_mpeg4_adv_core_l1, "mpeg4-adv-core-l1");
DECL_STR(_codec_video_mpeg4_adv_core_l2, "mpeg4-adv-core-l2");
DECL_STR(_codec_video_mpeg4_adv_scalable_texture_l1, "mpeg4-adv-scalable-texture-l1");
DECL_STR(_codec_video_mpeg4_adv_scalable_texture_l2, "mpeg4-adv-scalable-texture-l2");
DECL_STR(_codec_video_mpeg4_adv_scalable_texture_l3, "mpeg4-adv-scalable-texture-l3");
DECL_STR(_codec_video_mpeg4_simple_studio_l1, "mpeg4-simple-studio-l1");
DECL_STR(_codec_video_mpeg4_simple_studio_l2, "mpeg4-simple-studio-l2");
DECL_STR(_codec_video_mpeg4_simple_studio_l3, "mpeg4-simple-studio-l3");
DECL_STR(_codec_video_mpeg4_simple_studio_l4, "mpeg4-simple-studio-l4");
DECL_STR(_codec_video_mpeg4_core_studio_l1, "mpeg4-core-studio-l1");
DECL_STR(_codec_video_mpeg4_core_studio_l2, "mpeg4-core-studio-l2");
DECL_STR(_codec_video_mpeg4_core_studio_l3, "mpeg4-core-studio-l3");
DECL_STR(_codec_video_mpeg4_core_studio_l4, "mpeg4-core-studio-l4");
DECL_STR(_codec_video_mpeg4_adv_simple_l0, "mpeg4-adv-simple-l0");
DECL_STR(_codec_video_mpeg4_adv_simple_l1, "mpeg4-adv-simple-l1");
DECL_STR(_codec_video_mpeg4_adv_simple_l2, "mpeg4-adv-simple-l2");
DECL_STR(_codec_video_mpeg4_adv_simple_l3, "mpeg4-adv-simple-l3");
DECL_STR(_codec_video_mpeg4_adv_simple_l4, "mpeg4-adv-simple-l4");
DECL_STR(_codec_video_mpeg4_adv_simple_l5, "mpeg4-adv-simple-l5");
DECL_STR(_codec_video_mpeg4_adv_simple_l3b, "mpeg4-adv-simple-l3b");
DECL_STR(_codec_video_mpeg4_fgs_l0, "mpeg4-fgs-l0");
DECL_STR(_codec_video_mpeg4_fgs_l1, "mpeg4-fgs-l1");
DECL_STR(_codec_video_mpeg4_fgs_l2, "mpeg4-fgs-l2");
DECL_STR(_codec_video_mpeg4_fgs_l3, "mpeg4-fgs-l3");
DECL_STR(_codec_video_mpeg4_fgs_l4, "mpeg4-fgs-l4");
DECL_STR(_codec_video_mpeg4_fgs_l5, "mpeg4-fgs-l5");

DECL_STR(_codec_video_mpeg2_simple, "mpeg2-simple");
DECL_STR(_codec_video_mpeg2_main, "mpeg2-main");
DECL_STR(_codec_video_mpeg2_snr, "mpeg2-snr");
DECL_STR(_codec_video_mpeg2_spatial, "mpeg2-spatial");
DECL_STR(_codec_video_mpeg2_high, "mpeg2-high");
DECL_STR(_codec_video_mpeg2_422, "mpeg2-422");
DECL_STR(_codec_video_mpeg1, "mpeg1");
DECL_STR(_codec_video_jpeg, "jpeg");
DECL_STR(_codec_video_yuv12, "yuv12");
DECL_STR(_codec_video_h263, "h263");
DECL_STR(_codec_video_h261, "h261");

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

DECL_STR(_dlna_profile_aac_iso_320, "AAC_ISO_320");
DECL_STR(_dlna_profile_aac_iso, "AAC_ISO");
DECL_STR(_dlna_profile_aac_mult5_iso, "AAC_MULT5_ISO");
DECL_STR(_dlna_profile_ac3, "AC3");
DECL_STR(_dlna_profile_eac3, "EAC3");
DECL_STR(_dlna_profile_amr_3gpp, "AMR_3GPP");
DECL_STR(_dlna_profile_amr_wbplus, "AMR_WBplus");
DECL_STR(_dlna_profile_avc_mp4_bl_cif15_aac_520, "AVC_MP4_BL_CIF15_AAC_520");
DECL_STR(_dlna_profile_avc_mp4_bl_cif15_aac, "AVC_MP4_BL_CIF15_AAC");
DECL_STR(_dlna_profile_avc_mp4_bl_l3l_sd_aac, "AVC_MP4_BL_L3L_SD_AAC");
DECL_STR(_dlna_profile_avc_mp4_bl_l3_sd_aac, "AVC_MP4_BL_L3_SD_AAC");
DECL_STR(_dlna_profile_avc_mp4_mp_sd_aac_mult5, "AVC_MP4_MP_SD_AAC_MULT5");
DECL_STR(_dlna_profile_avc_mp4_mp_sd_mpeg1_l3, "AVC_MP4_MP_SD_MPEG1_L3");
DECL_STR(_dlna_profile_avc_mp4_mp_sd_ac3, "AVC_MP4_MP_SD_AC3");
DECL_STR(_dlna_profile_avc_mp4_mp_sd_eac3, "AVC_MP4_MP_SD_EAC3");
DECL_STR(_dlna_profile_avc_mp4_mp_hd_720p_aac, "AVC_MP4_MP_HD_720p_AAC");
DECL_STR(_dlna_profile_avc_mp4_mp_hd_1080i_aac, "AVC_MP4_MP_HD_1080i_AAC");
DECL_STR(_dlna_profile_avc_mkv_mp_hd_aac_mult5, "AVC_MKV_MP_HD_AAC_MULT5");
DECL_STR(_dlna_profile_avc_mkv_hp_hd_aac_mult5, "AVC_MKV_HP_HD_AAC_MULT5");
DECL_STR(_dlna_profile_avc_mkv_mp_hd_ac3, "AVC_MKV_MP_HD_AC3");
DECL_STR(_dlna_profile_avc_mkv_hp_hd_ac3, "AVC_MKV_HP_HD_AC3");
DECL_STR(_dlna_profile_avc_mkv_mp_hd_mpeg1_l3, "AVC_MKV_MP_HD_MPEG1_L3");
DECL_STR(_dlna_profile_avc_mkv_hp_hd_mpeg1_l3, "AVC_MKV_HP_HD_MPEG1_L3");
DECL_STR(_dlna_profile_avc_mp4_hp_hd_eac3, "AVC_MP4_HP_HD_EAC3");

DECL_STR(_dlna_mime_video, "video/mp4");
DECL_STR(_dlna_mime_video_3gp, "video/3gpp");
DECL_STR(_dlna_mime_video_matroska, "video/x-matroska");
DECL_STR(_dlna_mime_audio, "audio/mp4");
DECL_STR(_dlna_mime_audio_3gp, "audio/3gpp");
DECL_STR(_dlna_mime_audio_dolby, "audio/vnd.dolby.dd-raw");
DECL_STR(_dlna_mime_audio_eac3, "audio/eac3");

struct type_str {
    uint8_t type;
    const struct lms_string_size *str;
};

static const struct lms_string_size *_audio_codecs[] = {
    &_codec_audio_mpeg4aac_main,
    &_codec_audio_mpeg4aac_lc,
    &_codec_audio_mpeg4aac_ssr,
    &_codec_audio_mpeg4aac_ltp,
    &_codec_audio_mpeg4aac_he,
    &_codec_audio_mpeg4aac_scalable,
    &_codec_audio_mpeg4_twinvq,
    &_codec_audio_mpeg4_celp,
    &_codec_audio_mpeg4_hvxc,
    NULL,
    NULL,
    &_codec_audio_mpeg4_tssi,
    &_codec_audio_mpeg4_main_synthetic,
    &_codec_audio_mpeg4_wavetable_syn,
    &_codec_audio_mpeg4_general_midi,
    &_codec_audio_mpeg4_algo_syn_and_audio_fx,
    &_codec_audio_mpeg4_er_aac_lc,
    NULL,
    &_codec_audio_mpeg4_er_aac_ltp,
    &_codec_audio_mpeg4_er_aac_scalable,
    &_codec_audio_mpeg4_er_twinvq,
    &_codec_audio_mpeg4_er_bsac,
    &_codec_audio_mpeg4_er_acc_ld,
    &_codec_audio_mpeg4_er_celp,
    &_codec_audio_mpeg4_er_hvxc,
    &_codec_audio_mpeg4_er_hiln,
    &_codec_audio_mpeg4_er_parametric,
    &_codec_audio_mpeg4_ssc,
    &_codec_audio_mpeg4_ps,
    &_codec_audio_mpeg4_mpeg_surround,
    NULL,
    &_codec_audio_mpeg4_layer1,
    &_codec_audio_mpeg4_layer2,
    &_codec_audio_mpeg4_layer3,
    &_codec_audio_mpeg4_dst,
    &_codec_audio_mpeg4_audio_lossless,
    &_codec_audio_mpeg4_sls,
    &_codec_audio_mpeg4_sls_non_core
};

static const struct type_str _audio_types[] = {
    {MP4_MPEG2_AAC_MAIN_AUDIO_TYPE, &_codec_audio_mpeg2aac_main},
    {MP4_MPEG2_AAC_LC_AUDIO_TYPE, &_codec_audio_mpeg2aac_lc},
    {MP4_MPEG2_AAC_SSR_AUDIO_TYPE, &_codec_audio_mpeg2aac_ssr},
    {MP4_MPEG2_AUDIO_TYPE, &_codec_audio_mpeg2audio},
    {MP4_MPEG1_AUDIO_TYPE, &_codec_audio_mpeg1audio},
    {MP4_PCM16_LITTLE_ENDIAN_AUDIO_TYPE, &_codec_audio_pcm16le},
    {MP4_VORBIS_AUDIO_TYPE, &_codec_audio_vorbis},
    {MP4_ALAW_AUDIO_TYPE, &_codec_audio_alaw},
    {MP4_ULAW_AUDIO_TYPE, &_codec_audio_ulaw},
    {MP4_G723_AUDIO_TYPE, &_codec_audio_g723},
    {MP4_PCM16_BIG_ENDIAN_AUDIO_TYPE, &_codec_audio_pcm16be},
    {}
};

static const struct type_str _video_vprofiles[] = {
    {MPEG4_SP_L1, &_codec_video_mpeg4_simple_l1},
    {MPEG4_SP_L2, &_codec_video_mpeg4_simple_l2},
    {MPEG4_SP_L3, &_codec_video_mpeg4_simple_l3},
    {MPEG4_SP_L0, &_codec_video_mpeg4_simple_l0},
    {MPEG4_SSP_L1, &_codec_video_mpeg4_simple_scalable_l1},
    {MPEG4_SSP_L2, &_codec_video_mpeg4_simple_scalable_l2},
    {MPEG4_CP_L1, &_codec_video_mpeg4_core_l1},
    {MPEG4_CP_L2, &_codec_video_mpeg4_core_l2},
    {MPEG4_MP_L2, &_codec_video_mpeg4_main_l2},
    {MPEG4_MP_L3, &_codec_video_mpeg4_main_l3},
    {MPEG4_MP_L4, &_codec_video_mpeg4_main_l4},
    {MPEG4_NBP_L2, &_codec_video_mpeg4_nbit_l2},
    {MPEG4_STP_L1, &_codec_video_mpeg4_scalable_texture_l1},
    {MPEG4_SFAP_L1, &_codec_video_mpeg4_simple_face_anim_l1},
    {MPEG4_SFAP_L2, &_codec_video_mpeg4_simple_face_anim_l2},
    {MPEG4_SFBAP_L1, &_codec_video_mpeg4_simple_fba_l1},
    {MPEG4_SFBAP_L2, &_codec_video_mpeg4_simple_fba_l2},
    {MPEG4_BATP_L1, &_codec_video_mpeg4_basic_anim_text_l1},
    {MPEG4_BATP_L2, &_codec_video_mpeg4_basic_anim_text_l2},
    {MPEG4_HP_L1, &_codec_video_mpeg4_hybrid_l1},
    {MPEG4_HP_L2, &_codec_video_mpeg4_hybrid_l2},
    {MPEG4_ARTSP_L1, &_codec_video_mpeg4_adv_rt_simple_l1},
    {MPEG4_ARTSP_L2, &_codec_video_mpeg4_adv_rt_simple_l2},
    {MPEG4_ARTSP_L3, &_codec_video_mpeg4_adv_rt_simple_l3},
    {MPEG4_ARTSP_L4, &_codec_video_mpeg4_adv_rt_simple_l4},
    {MPEG4_CSP_L1, &_codec_video_mpeg4_core_scalable_l1},
    {MPEG4_CSP_L2, &_codec_video_mpeg4_core_scalable_l2},
    {MPEG4_CSP_L3, &_codec_video_mpeg4_core_scalable_l3},
    {MPEG4_ACEP_L1, &_codec_video_mpeg4_adv_coding_efficiency_l1},
    {MPEG4_ACEP_L2, &_codec_video_mpeg4_adv_coding_efficiency_l2},
    {MPEG4_ACEP_L3, &_codec_video_mpeg4_adv_coding_efficiency_l3},
    {MPEG4_ACEP_L4, &_codec_video_mpeg4_adv_coding_efficiency_l4},
    {MPEG4_ACP_L1, &_codec_video_mpeg4_adv_core_l1},
    {MPEG4_ACP_L2, &_codec_video_mpeg4_adv_core_l2},
    {MPEG4_AST_L1, &_codec_video_mpeg4_adv_scalable_texture_l1},
    {MPEG4_AST_L2, &_codec_video_mpeg4_adv_scalable_texture_l2},
    {MPEG4_AST_L3, &_codec_video_mpeg4_adv_scalable_texture_l3},
    {MPEG4_S_STUDIO_P_L1, &_codec_video_mpeg4_simple_studio_l1},
    {MPEG4_S_STUDIO_P_L2, &_codec_video_mpeg4_simple_studio_l2},
    {MPEG4_S_STUDIO_P_L3, &_codec_video_mpeg4_simple_studio_l3},
    {MPEG4_S_STUDIO_P_L4, &_codec_video_mpeg4_simple_studio_l4},
    {MPEG4_C_STUDIO_P_L1, &_codec_video_mpeg4_core_studio_l1},
    {MPEG4_C_STUDIO_P_L2, &_codec_video_mpeg4_core_studio_l2},
    {MPEG4_C_STUDIO_P_L3, &_codec_video_mpeg4_core_studio_l3},
    {MPEG4_C_STUDIO_P_L4, &_codec_video_mpeg4_core_studio_l4},
    {MPEG4_ASP_L0, &_codec_video_mpeg4_adv_simple_l0},
    {MPEG4_ASP_L1, &_codec_video_mpeg4_adv_simple_l1},
    {MPEG4_ASP_L2, &_codec_video_mpeg4_adv_simple_l2},
    {MPEG4_ASP_L3, &_codec_video_mpeg4_adv_simple_l3},
    {MPEG4_ASP_L4, &_codec_video_mpeg4_adv_simple_l4},
    {MPEG4_ASP_L5, &_codec_video_mpeg4_adv_simple_l5},
    {MPEG4_ASP_L3B, &_codec_video_mpeg4_adv_simple_l3b},
    {MPEG4_FGSP_L0, &_codec_video_mpeg4_fgs_l0},
    {MPEG4_FGSP_L1, &_codec_video_mpeg4_fgs_l1},
    {MPEG4_FGSP_L2, &_codec_video_mpeg4_fgs_l2},
    {MPEG4_FGSP_L3, &_codec_video_mpeg4_fgs_l3},
    {MPEG4_FGSP_L4, &_codec_video_mpeg4_fgs_l4},
    {MPEG4_FGSP_L5, &_codec_video_mpeg4_fgs_l5},
    {}
};

static const struct type_str _video_types[] = {
    {MP4_MPEG2_SIMPLE_VIDEO_TYPE, &_codec_video_mpeg2_simple},
    {MP4_MPEG2_MAIN_VIDEO_TYPE, &_codec_video_mpeg2_main},
    {MP4_MPEG2_SNR_VIDEO_TYPE, &_codec_video_mpeg2_snr},
    {MP4_MPEG2_SPATIAL_VIDEO_TYPE, &_codec_video_mpeg2_spatial},
    {MP4_MPEG2_HIGH_VIDEO_TYPE, &_codec_video_mpeg2_high},
    {MP4_MPEG2_442_VIDEO_TYPE, &_codec_video_mpeg2_422},
    {MP4_MPEG1_VIDEO_TYPE, &_codec_video_mpeg1},
    {MP4_JPEG_VIDEO_TYPE, &_codec_video_jpeg},
    {MP4_YUV12_VIDEO_TYPE, &_codec_video_yuv12},
    {MP4_H263_VIDEO_TYPE, &_codec_video_h263},
    {MP4_H261_VIDEO_TYPE, &_codec_video_h261},
    {}
};


static const char _name[] = "mp4";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".mp4"),
    LMS_STATIC_STRING_SIZE(".m4a"),
    LMS_STATIC_STRING_SIZE(".m4v"),
    LMS_STATIC_STRING_SIZE(".mov"),
    LMS_STATIC_STRING_SIZE(".qt"),
    LMS_STATIC_STRING_SIZE(".3gp")
};
static const char *_cats[] = {
    "multimedia",
    "audio",
    "video",
    NULL
};
static const char *_authors[] = {
    "Andre Moreira Magalhaes",
    "Lucas De Marchi",
    "Gustavo Sverzut Barbieri",
    "Leandro Dorileo",
    NULL
};

static const struct lms_string_size nullstr = { };

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

static inline struct lms_string_size
_find_type_str(const struct type_str *base, uint8_t type)
{
    const struct type_str *itr;

    for (itr = base; itr->str != NULL; itr++) {
        if (itr->type == type)
            return *itr->str;
    }

    return nullstr;
}

static struct lms_string_size
_get_audio_codec(MP4FileHandle mp4_fh, MP4TrackId id)
{
    const char *data_name = MP4GetTrackMediaDataName(mp4_fh, id);
    uint8_t type;

    if (!data_name)
        return nullstr;
    if (strcasecmp(data_name, "samr") == 0)
        return _codec_audio_amr;
    if (strcasecmp(data_name, "sawb") == 0)
        return _codec_audio_amr_wb;
    if (strcasecmp(data_name, "mp4a") != 0)
        return nullstr;

    type = MP4GetTrackEsdsObjectTypeId(mp4_fh, id);
    if (type == MP4_MPEG4_AUDIO_TYPE) {
        type = MP4GetTrackAudioMpeg4Type(mp4_fh, id);
        if (type == 0 || type > LMS_ARRAY_SIZE(_audio_codecs) ||
            _audio_codecs[type - 1] == NULL)
            return nullstr;

        return *_audio_codecs[type - 1];
    }

    return _find_type_str(_audio_types, type);
}

/* WARNING: returned str is malloc()'ed if not NULL, use free() */
static struct lms_string_size
_get_video_codec(MP4FileHandle mp4_fh, MP4TrackId id)
{
    const char *data_name = MP4GetTrackMediaDataName(mp4_fh, id);
    struct lms_string_size ret = {}, tmp;
    char buf[256];
    char ofmt[8] = {};

    if (!data_name)
        return nullstr;

    if (strcasecmp(data_name, "encv") == 0) {
        if (!MP4GetTrackMediaDataOriginalFormat(mp4_fh, id, ofmt, sizeof(ofmt)))
            return nullstr;
    }

    if (strcasecmp(data_name, "s263") == 0) {
        ret = _codec_video_h263;
        goto found;
    }

    if (strcasecmp(data_name, "avc1") == 0 ||
        strcasecmp(ofmt, "264b") == 0) {
        uint8_t profile, level;
        char str_profile[64], str_level[64];

        if (!MP4GetTrackH264ProfileLevel(mp4_fh, id, &profile, &level)) {
            return nullstr;
        }

        switch (profile) {
        case 66:
            memcpy(str_profile, "baseline", sizeof("baseline"));
            break;
        case 77:
            memcpy(str_profile, "main", sizeof("main"));
            break;
        case 88:
            memcpy(str_profile, "extended", sizeof("extended"));
            break;
        case 100:
            memcpy(str_profile, "high", sizeof("high"));
            break;
        case 110:
            memcpy(str_profile, "high-10", sizeof("high-10"));
            break;
        case 122:
            memcpy(str_profile, "high-422", sizeof("high-422"));
            break;
        case 144:
            memcpy(str_profile, "high-444", sizeof("high-444"));
            break;
        default:
            snprintf(str_profile, sizeof(str_profile), "unknown-%d", profile);
        }

        if (level % 10 == 0)
            snprintf(str_level, sizeof(str_level), "%u", level / 10);
        else
            snprintf(str_level, sizeof(str_level), "%u.%u", level / 10,
                     level % 10);

        /* fix constrained and 1b case for baseline and main */
        if (profile == 66 || profile == 77) {
            uint8_t *sps;
            uint32_t spslen;
            bool constrained = false;
            bool level1b = false;

            if (MP4HaveAtom(mp4_fh,
                            "moov.trak.mdia.minf.stbl.stsd.avc1.avcC") &&
                MP4GetBytesProperty(mp4_fh, "moov.trak.mdia.minf.stbl.stsd."
                                    "avc1.avcC.sequenceEntries."
                                    "sequenceParameterSetNALUnit",
                                    &sps, &spslen)) {
                /* SPS (Sequence Parameter Set) is:
                 *  8 bits (1 byte) for profile_idc
                 *  1 bit for constraint_set0_flag
                 *  1 bit for constraint_set1_flag <- we use this for constr.
                 *  1 bit for constraint_set2_flag
                 *  1 bit for constraint_set3_flag <- we use this for 1b
                 * based on ffmpeg's (libavcodec/h264_ps.c) and
                 * x264 (encoder/set.c)
                 */
                if (spslen > 1) {
                    if ((sps[1] >> 1) & 0x1)
                        constrained = true;
                    if (((sps[1] >> 3) & 0x1) && level / 10 == 1)
                        level1b = true;
                }
                free(sps);

                if (constrained) {
                    if (profile == 66)
                        memcpy(str_profile, "constrained-baseline",
                               sizeof("constrained-baseline"));
                    else
                        memcpy(str_profile, "constrained-main",
                               sizeof("constrained-main"));
                }

                if (level1b)
                    memcpy(str_level, "1b", sizeof("1b"));
            }
        }

        ret.len = snprintf(buf, sizeof(buf), "h264-%s-l%s",
                           str_profile, str_level);
        ret.str = buf;
        goto found;
    } else if (strcasecmp(data_name, "mp4v") == 0 ||
             strcasecmp(data_name, "encv") == 0) {
        uint8_t type = MP4GetTrackEsdsObjectTypeId(mp4_fh, id);

        if (type == MP4_MPEG4_VIDEO_TYPE) {
            type = MP4GetVideoProfileLevel(mp4_fh, id);
            ret = _find_type_str(_video_vprofiles, type);
        } else
            ret = _find_type_str(_video_types, type);
        goto found;
    }

    return nullstr;

found: /* ugly, but h264 codec is composed in runtime */

    if (!lms_string_size_dup(&tmp, &ret))
        return nullstr;
    return tmp;
}

static struct lms_string_size
_get_lang(MP4FileHandle mp4_fh, MP4TrackId id)
{
    struct lms_string_size ret;
    char buf[4];

    if (!MP4GetTrackLanguage(mp4_fh, id, buf))
        return nullstr;

    if (memcmp(buf, "und", 4) == 0)
        return nullstr;

    if (!lms_string_size_strndup(&ret, buf, -1))
        return nullstr;

    return ret;
}

static struct lms_string_size
_get_container(MP4FileHandle mp4_fh)
{
    const char *brand;

    if (!MP4GetStringProperty(mp4_fh, "ftyp.majorBrand", &brand))
        return _container_mp4;

    if (strncasecmp(brand, "3gp", 3) == 0)
        return _container_3gp;

    /* NOTE: this should be an array, but the C wrapper of mp4v2
     * doesn't expose a way to give the index number and
     * ftyp.compatibleBrands is not in counted format (name[idx])
     */
    if (!MP4GetStringProperty(mp4_fh, "ftyp.compatibleBrands", &brand))
        return _container_mp4;

    if (strncasecmp(brand, "3gp", 3) == 0)
        return _container_3gp;

    return _container_mp4;
}

#define DLNA_VIDEO_RES(_width, _height)                                 \
    &(struct dlna_video_res) {.width = _width, .height = _height}       \

#define DLNA_VIDEO_RES_RANGE(_wmin, _wmax, _hmin, _hmax)                \
    &(struct dlna_video_res_range) {.width_min = _wmin,                 \
            .width_max = _wmax, .height_min = _hmin,                    \
            .height_max = _hmax}                                        \

#define DLNA_BITRATE(_min, _max)                            \
    &(struct dlna_bitrate) {.min = _min, .max = _max}       \

#define DLNA_LEVEL(_val...)                                 \
    &(struct dlna_level) {.levels = {_val, NULL}}           \

#define DLNA_VIDEO_FRAMERATE_RANGE(_min, _max)                          \
    &(struct dlna_video_framerate_range) {.min = _min, .max = _max}     \

#define DLNA_AUDIO_RATE(_val...)                        \
    &(struct dlna_audio_rate) {.rates = {_val}}         \

#define MAX_AUDIO_LEVELS 5
#define MAX_AUDIO_MPEG_VERSIONS 2
#define MAX_AUDIO_RATES 9
#define MAX_VIDEO_RULE_LEVEL 13

struct dlna_bitrate {
     unsigned int min;
     unsigned int max;
};

struct dlna_video_framerate_range {
    double min;
    double max;
};

struct dlna_video_res {
    unsigned int width;
    unsigned int height;
};

struct dlna_video_res_range {
    unsigned int width_min;
    unsigned int width_max;
    unsigned int height_min;
    unsigned int height_max;
};

struct dlna_level {
    const char *levels[MAX_VIDEO_RULE_LEVEL];
};

struct dlna_video_rule {
     struct dlna_video_res *res;
     struct dlna_video_res_range *res_range;
     struct dlna_bitrate *bitrate;
     struct dlna_level *levels;
     struct dlna_video_framerate_range *framerate_range;
};

struct dlna_audio_rate {
     unsigned int rates[MAX_AUDIO_RATES];
};

struct dlna_audio_rule {
     const struct lms_string_size *codec;
     const struct lms_string_size *container;
     struct dlna_audio_rate *rates;
     struct dlna_level *levels;
     struct dlna_bitrate *channels;
     struct dlna_bitrate *bitrate;
};

struct dlna_video_profile {
     const struct lms_string_size *dlna_profile;
     const struct lms_string_size *dlna_mime;
     const struct dlna_video_rule *video_rules;
     const struct dlna_audio_rule *audio_rule;
};

struct dlna_audio_profile {
     const struct lms_string_size *dlna_profile;
     const struct lms_string_size *dlna_mime;
     const struct dlna_audio_rule *audio_rule;
};

static const struct dlna_video_rule _dlna_video_rule_sp_l3[] = {
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

static const struct dlna_video_rule _dlna_video_rule_avc_mp4_mp_sd[] = {
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

#define DLNA_VIDEO_LEVELS_MPEG4_P2_MP4_SP_VGA_AAC \
    "0", "0b", "1", "2", "3"                      \

static const struct dlna_video_rule _dlna_video_mpeg4_p2_mp4_sp_vga_aac[] = {
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

static const struct dlna_video_rule _dlna_video_mpeg4_p2_mp4_sp_l2_aac[] = {
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
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 360),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 576),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 480),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 288),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 240),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 240),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 180),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(240, 180),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(208, 160),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 144),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 120),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 120),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 112),                \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 90),                 \
        .bitrate = DLNA_BITRATE(1, 64000),              \
        .levels = DLNA_LEVEL("0", "1"),                 \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 480),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 360),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 576),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 480),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 288),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 240),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 240),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 180),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(240, 180),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(208, 160),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 144),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 120),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 120),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 112),                \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 90),                 \
        .bitrate = DLNA_BITRATE(1, 128000),             \
        .levels = DLNA_LEVEL("0b", "2"),                \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 480),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 360),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 576),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 480),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 288),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 240),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 240),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 180),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(240, 180),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(208, 160),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 144),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 120),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 120),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 112),                \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 90),                 \
        .bitrate = DLNA_BITRATE(1, 384000),             \
        .levels = DLNA_LEVEL("3"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 480),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5"),                      \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(640, 360),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 576),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(720, 480),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 288),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(352, 240),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 240),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(320, 180),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(240, 180),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(208, 160),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 144),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(176, 120),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 120),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 112),                \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    },                                                  \
    {                                                   \
        .res = DLNA_VIDEO_RES(160, 90),                 \
        .bitrate = DLNA_BITRATE(1, 8000000),            \
        .levels = DLNA_LEVEL("5")                       \
    }                                                   \

static const struct dlna_video_rule _dlna_video_rule_sp_l5[] = {
    DLNA_VIDEO_RULE_SP_L5,
    { NULL },
};

static const struct dlna_video_rule _dlna_video_rule_sp_l6[] = {
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
        .bitrate = DLNA_BITRATE(1, 64000),      \
        .levels = DLNA_LEVEL("0")               \
    },                                          \
    {                                           \
        .res = DLNA_VIDEO_RES(128, 96),         \
        .bitrate = DLNA_BITRATE(1, 64000),      \
        .levels = DLNA_LEVEL("0")               \
    }                                           \

static const struct dlna_video_rule _dlna_video_rule_mpeg4_h263_mp4_p0_l10_aac[] = {
    DLNA_VIDEO_RULE_H263_P0_L10,
    { NULL },
};

static const struct dlna_video_rule _dlna_video_rule_mpeg4_h263_mp4_p0_l10_aac_ltp[] = {
    DLNA_VIDEO_RULE_H263_P0_L10,
    { NULL },
};

#define DLNA_VIDEO_LEVELS_CIF                   \
    "1", "1b", "1.1", "1.2"                     \

#define DLNA_VIDEO_RULES_AVC_MP4_BL_CIF15                               \
    {                                                                   \
        .res = DLNA_VIDEO_RES(352, 288),                                \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 15/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(352, 240),                                \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 18/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(320, 240),                                \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 20/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(320, 180),                                \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 26/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(240, 180),                                \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(208, 160),                                \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(176, 144),                                \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(176, 120),                                \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(160, 120),                                \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(160, 112),                                \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(160, 90),                                 \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(128, 96),                                 \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(240, 135),                                \
        .bitrate = DLNA_BITRATE(1, 384000),                             \
        .levels = DLNA_LEVEL(DLNA_VIDEO_LEVELS_CIF),                    \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),       \
    }                                                                   \

static const struct dlna_video_rule _dlna_video_rule_avc_mp4_bl_cif15_aac_520[] = {
    DLNA_VIDEO_RULES_AVC_MP4_BL_CIF15,
    { NULL },
};

static const struct dlna_video_rule _dlna_video_rule_avc_mp4_bl_cif15_aac[] = {
    DLNA_VIDEO_RULES_AVC_MP4_BL_CIF15,
    { NULL },
};

#define DLNA_VIDEO_RULES_BL_L3L_SD_AAC                          \
    "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3"      \

// common between l3 and l3l sd aac
#define DLNA_VIDEO_RULES_L3_L3L_SD_AAC                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(720, 576),                                \
        .bitrate = DLNA_BITRATE(1, 4500000),                            \
        .levels = DLNA_LEVEL(DLNA_VIDEO_RULES_BL_L3L_SD_AAC),           \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 25/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(720, 480),                                \
        .bitrate = DLNA_BITRATE(1, 4500000),                            \
        .levels = DLNA_LEVEL(DLNA_VIDEO_RULES_BL_L3L_SD_AAC),           \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30000/1001), \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(640, 480),                                \
        .bitrate = DLNA_BITRATE(1, 4500000),                            \
        .levels = DLNA_LEVEL(DLNA_VIDEO_RULES_BL_L3L_SD_AAC),           \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),       \
    },                                                                  \
    {                                                                   \
        .res = DLNA_VIDEO_RES(640, 360),                                \
        .bitrate = DLNA_BITRATE(1, 4500000),                            \
        .levels = DLNA_LEVEL(DLNA_VIDEO_RULES_BL_L3L_SD_AAC),           \
        .framerate_range = DLNA_VIDEO_FRAMERATE_RANGE(0/1, 30/1),       \
    }                                                                   \

static const struct dlna_video_rule _dlna_video_rule_avc_mp4_bl_l3l_sd_aac[] = {
    DLNA_VIDEO_RULES_L3_L3L_SD_AAC,
    { NULL },
};

static const struct dlna_video_rule _dlna_video_rule_avc_mp4_bl_l3_sd_aac[] = {
    DLNA_VIDEO_RULES_L3_L3L_SD_AAC,
    { NULL },
};

#define DLNA_VIDEO_LEVELS_SD_EAC3                               \
    "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3"      \

static const struct dlna_video_rule _dlna_video_rule_avc_mp4_mp_sd_eac3[] = {
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
};

#define DLNA_VIDEO_LEVELS_MP_HD_720P                                    \
    "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3", "3.1"       \

static const struct dlna_video_rule _dlna_video_rule_avc_mp4_mp_hd_720p_aac[] = {
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
};

#define DLNA_VIDEO_LEVELS_MP_HD_1080I                                   \
    "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3", "3.1", "3.2", "4" \

static const struct dlna_video_rule _dlna_video_rule_avc_mp4_mp_hd_1080i_aac[] = {
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
};

#define DLNA_VIDEO_LEVELS_AVC_MKV_HP_HD                                 \
    "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3", "3.1", "3.2", "4" \

static const struct dlna_video_rule _dlna_video_avc_mkv_hp_hd[] = {
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
};

// mpeg4
static const struct dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 216000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_aac_ltp = {
    .codec = &_codec_audio_mpeg4aac_ltp,
    .bitrate = DLNA_BITRATE(1, 216000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_vga_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 256000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_l2_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 128000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_l5_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_mpeg4_p2_mp4_sp_l6_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_mpeg4_h263_mp4_p0_l10_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 86000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_mpeg4_h263_mp4_p0_l10_aac_ltp = {
    .codec = &_codec_audio_mpeg4aac_ltp,
    .bitrate = DLNA_BITRATE(1, 86000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

// avc
static const struct dlna_audio_rule _dlna_audio_rule_avc_mp4_bl_cif15_aac_520 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 128000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mp4_bl_cif15_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 200000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mp4_bl_l3l_sd_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 256000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mp4_bl_l3_sd_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(1, 256000),
    .levels = DLNA_LEVEL("1", "2"),
    .channels = DLNA_BITRATE(1, 2),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_sd_aac_mult5 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0,1440000),
    .levels = DLNA_LEVEL("1", "2", "4"),
    .channels = DLNA_BITRATE(1, 6),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_sd_mpeg1_l3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(32000, 32000),
    .channels = DLNA_BITRATE(1, 2),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_sd_ac3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(64000, 64000),
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_sd_eac3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0, 3024000),
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_hd_720p_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0, 576000),
    .channels = DLNA_BITRATE(1, 2),
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mp4_mp_hd_1080i_aac = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0, 576000),
    .channels = DLNA_BITRATE(1, 2),
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mkv_mp_hd_aac_mult5 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0, 1440000),
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000),
    .levels = DLNA_LEVEL("1", "2", "4"),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mkv_mp_hd_ac3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .bitrate = DLNA_BITRATE(0, 1440000),
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
};

static const struct dlna_audio_rule _dlna_audio_rule_avc_mkv_mp_hd_mpeg1_l3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .channels = DLNA_BITRATE(1, 2),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(32000, 320000),
};

static const  struct dlna_video_profile _dlna_video_profile_rules[] = {
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
};

#define DLNA_AUDIO_RULE_AAC_ISO                                         \
    .codec = &_codec_audio_mpeg4aac_lc,                                 \
    .channels = DLNA_BITRATE(1, 2),                                     \
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000,                 \
                22050, 24000, 32000, 44100, 48000),                     \
    .bitrate = DLNA_BITRATE(0, 576000)                                  \

static const struct dlna_audio_rule _dlna_audio_rule_aac_iso = {
    DLNA_AUDIO_RULE_AAC_ISO,
    .container = &_container_mp4,
};

static const struct dlna_audio_rule _dlna_audio_rule_aac_iso_3gp = {
    DLNA_AUDIO_RULE_AAC_ISO,
    .container = &_container_3gp,
};

#define DLNA_AUDIO_RULE_AAC_ISO_320                                     \
    .codec = &_codec_audio_mpeg4aac_lc,                                 \
    .channels = DLNA_BITRATE(1, 2),                                     \
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000,                 \
                22050, 24000, 32000, 44100, 48000),                     \
    .bitrate = DLNA_BITRATE(0, 320000)                                  \

static const struct dlna_audio_rule _dlna_audio_rule_aac_iso_320 = {
    DLNA_AUDIO_RULE_AAC_ISO_320,
    .container = &_container_mp4,
};

static const struct dlna_audio_rule _dlna_audio_rule_aac_iso_320_3gp = {
    DLNA_AUDIO_RULE_AAC_ISO_320,
    .container = &_container_3gp,
};

#define DLNA_AUDIO_RULE_AAC_MULT5_ISO                                   \
    .codec = &_codec_audio_mpeg4aac_lc,                                 \
    .channels = DLNA_BITRATE(1, 6),                                     \
    .rates = DLNA_AUDIO_RATE(8000, 11025, 12000, 16000,                 \
                22050, 24000, 32000, 44100, 48000),                     \
    .bitrate = DLNA_BITRATE(0, 1440000)                                 \

static const struct dlna_audio_rule _dlna_audio_rule_aac_mult5_iso = {
    DLNA_AUDIO_RULE_AAC_MULT5_ISO,
    .container = &_container_mp4,
};

static const struct dlna_audio_rule _dlna_audio_rule_aac_mult5_iso_3gp = {
    DLNA_AUDIO_RULE_AAC_MULT5_ISO,
    .container = &_container_3gp,
};

static const struct dlna_audio_rule _dlna_audio_rule_ac3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
    .bitrate = DLNA_BITRATE(64000, 64000),
};

static const struct dlna_audio_rule _dlna_audio_rule_eac3 = {
    .codec = &_codec_audio_mpeg4aac_lc,
    .channels = DLNA_BITRATE(1, 6),
    .rates = DLNA_AUDIO_RATE(32000, 44100, 48000),
    .bitrate = DLNA_BITRATE( 32000, 6144000),
};

static const struct dlna_audio_rule _dlna_audio_rule_amr_3gpp = {
    .codec = &_codec_audio_amr,
    .container = &_container_3gp,
};

static const struct dlna_audio_rule _dlna_audio_rule_amr = {
    .codec = &_codec_audio_amr,
};

static const struct dlna_audio_rule _dlna_audio_rule_amr_wbplus = {
    .codec = &_codec_audio_amr_wb,
    .rates = DLNA_AUDIO_RATE(8000, 16000, 24000, 32000, 48000),
};

static const struct dlna_audio_rule _dlna_audio_rule_amr_wbplus_3gp = {
    .codec = &_codec_audio_amr_wb,
    .container = &_container_3gp,
    .rates = DLNA_AUDIO_RATE(8000, 16000, 24000, 32000, 48000),
};

//_dlna_profile_aac_mult5_iso
static const  struct dlna_audio_profile _dlna_audio_profile_rules[] = {
    {
        .dlna_profile = &_dlna_profile_aac_iso,
        .dlna_mime = &_dlna_mime_audio,
        .audio_rule = &_dlna_audio_rule_aac_iso,
    },
    {
        .dlna_profile = &_dlna_profile_aac_iso,
        .dlna_mime = &_dlna_mime_audio_3gp,
        .audio_rule = &_dlna_audio_rule_aac_iso_3gp,
    },
    {
        .dlna_profile = &_dlna_profile_aac_iso_320,
        .dlna_mime = &_dlna_mime_audio,
        .audio_rule = &_dlna_audio_rule_aac_iso_320,
    },
    {
        .dlna_profile = &_dlna_profile_aac_iso_320,
        .dlna_mime = &_dlna_mime_audio_3gp,
        .audio_rule = &_dlna_audio_rule_aac_iso_320_3gp,
    },
    {
        .dlna_profile = &_dlna_profile_aac_mult5_iso,
        .dlna_mime = &_dlna_mime_audio,
        .audio_rule = &_dlna_audio_rule_aac_mult5_iso,
    },
    {
        .dlna_profile = &_dlna_profile_aac_mult5_iso,
        .dlna_mime = &_dlna_mime_audio_3gp,
        .audio_rule = &_dlna_audio_rule_aac_mult5_iso_3gp,
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
        .dlna_mime = &_dlna_mime_audio_3gp,
        .audio_rule = &_dlna_audio_rule_amr_3gpp,
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
    {
        .dlna_profile = &_dlna_profile_amr_wbplus,
        .dlna_mime = &_dlna_mime_audio_3gp,
        .audio_rule = &_dlna_audio_rule_amr_wbplus_3gp,
    },
    { NULL }
};

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

static bool
_uint_vector_has_value(const unsigned int *list, unsigned int wanted)
{
    int i;
    unsigned int curr;

    for (i = 0, curr = list[i]; curr != INT32_MAX; i++, curr = list[i])
      if (curr == wanted) return true;

    return false;
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

        if (rule->bitrate && (info->bitrate < rule->bitrate->min ||
                              info->bitrate > rule->bitrate->max))
            continue;

        if (rule->rates &&
            !_uint_vector_has_value(rule->rates->rates, info->sampling_rate))
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

static const struct dlna_video_profile *
_get_video_dlna_profile(const struct lms_stream_audio_info *audio,
                        const struct lms_stream *audio_stream,
                        const struct lms_stream_video_info *video,
                        const struct lms_stream *video_stream)
{
    int i, length;
    const struct dlna_video_profile *curr;
    char *level;

    level = strstr(video_stream->codec.str, "-l");
    length = sizeof(_dlna_video_profile_rules) / sizeof(struct dlna_video_profile);

    for (i = 0, curr = &_dlna_video_profile_rules[i]; i < length; i++,
             curr = &_dlna_video_profile_rules[i]) {
        const struct dlna_video_rule *video_rule;
        const struct dlna_audio_rule *audio_rule;
        const struct dlna_video_rule *r;

        audio_rule = curr->audio_rule;
        r = curr->video_rules;

        if (audio_rule->bitrate && (audio->bitrate < audio_rule->bitrate->min ||
                                    audio->bitrate > audio_rule->bitrate->max))
            continue;

        if (audio_rule->rates &&
            !_uint_vector_has_value(audio_rule->rates->rates,
                                    audio->sampling_rate))
            continue;

        if (audio_rule->channels &&
            (audio->channels < audio_rule->channels->min ||
             audio->channels > audio_rule->channels->max))
            continue;

        if (audio_rule->codec &&
            strcmp(audio_stream->codec.str, audio_rule->codec->str))
            continue;

        while (true) {
            video_rule = r++;

            if (!video_rule->res && !video_rule->bitrate &&
                !video_rule->levels && !video_rule->framerate_range)
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

            if (video_rule->framerate_range &&
                !(video->framerate >= video_rule->framerate_range->min &&
                  video->framerate <= video_rule->framerate_range->max))
                continue;

            if (video_rule->bitrate &&
                !(video->bitrate >= video_rule->bitrate->min &&
                  video->bitrate <= video_rule->bitrate->max))
                continue;

            if (video_rule->levels &&
                !_string_vector_has_value(video_rule->levels->levels,
                                          level + 2))
                continue;

            return curr;
        }
    }

    return NULL;
}

static void
_fill_video_dlna_profile(struct lms_video_info *info)
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

    profile = _get_video_dlna_profile(audio, audio_stream, video, video_stream);
    if (!profile) return;

    info->dlna_profile = *profile->dlna_profile;
    info->dlna_mime = *profile->dlna_mime;
}

static int
_parse(struct plugin *plugin, struct lms_context *ctxt, const struct lms_file_info *finfo, void *match)
{
    struct mp4_info info = { };
    struct lms_audio_info audio_info = { };
    struct lms_video_info video_info = { };
    int r, stream_type = LMS_STREAM_TYPE_AUDIO;
    MP4FileHandle mp4_fh;
    u_int32_t num_tracks, i;
    const MP4Tags *tags;

    mp4_fh = MP4Read(finfo->path);
    if (mp4_fh == MP4_INVALID_FILE_HANDLE) {
        fprintf(stderr, "ERROR: cannot read mp4 file %s\n", finfo->path);
        return -1;
    }

    tags = MP4TagsAlloc();
    if (!tags)
        return -1;

    if (!MP4TagsFetch(tags, mp4_fh)) {
        r = -1;
        goto fail;
    }

    lms_string_size_strndup(&info.title, tags->name, -1);
    lms_string_size_strndup(&info.artist, tags->artist, -1);

    /* check if the file contains a video track */
    num_tracks = MP4GetNumberOfTracks(mp4_fh, MP4_VIDEO_TRACK_TYPE, 0);
    if (num_tracks > 0)
        stream_type = LMS_STREAM_TYPE_VIDEO;

    info.length = MP4GetDuration(mp4_fh) /
        MP4GetTimeScale(mp4_fh) ?: 1;

    if (stream_type == LMS_STREAM_TYPE_AUDIO) {
        MP4TrackId id;

        lms_string_size_strndup(&info.album, tags->album, -1);
        lms_string_size_strndup(&info.genre, tags->genre, -1);
        if (tags->track)
            info.trackno = tags->track->index;

        id = MP4FindTrackId(mp4_fh, 0, MP4_AUDIO_TRACK_TYPE, 0);
        audio_info.bitrate = MP4GetTrackBitRate(mp4_fh, id);
        audio_info.channels = MP4GetTrackAudioChannels(mp4_fh, id);
        audio_info.sampling_rate = MP4GetTrackTimeScale(mp4_fh, id);
        audio_info.length = info.length;
        audio_info.codec = _get_audio_codec(mp4_fh, id);
    } else {
        num_tracks = MP4GetNumberOfTracks(mp4_fh, NULL, 0);
        for (i = 0; i < num_tracks; i++) {
            MP4TrackId id = MP4FindTrackId(mp4_fh, i, NULL, 0);
            const char *type = MP4GetTrackType(mp4_fh, id);
            enum lms_stream_type lmstype;
            struct lms_stream *s;

            if (strcmp(type, MP4_AUDIO_TRACK_TYPE) == 0)
                lmstype = LMS_STREAM_TYPE_AUDIO;
            else if (strcmp(type, MP4_VIDEO_TRACK_TYPE) == 0)
                lmstype = LMS_STREAM_TYPE_VIDEO;
            else
                continue;

            s = calloc(1, sizeof(*s));
            s->type = lmstype;
            s->stream_id = id;
            s->lang = _get_lang(mp4_fh, id);

            if (lmstype == LMS_STREAM_TYPE_AUDIO) {
                s->codec = _get_audio_codec(mp4_fh, id);
                s->audio.sampling_rate = MP4GetTrackTimeScale(mp4_fh, id);
                s->audio.bitrate = MP4GetTrackBitRate(mp4_fh, id);
                s->audio.channels = MP4GetTrackAudioChannels(mp4_fh, id);
            } else if (lmstype == LMS_STREAM_TYPE_VIDEO) {
                s->codec = _get_video_codec(mp4_fh, id); /* malloc() */
                s->video.bitrate = MP4GetTrackBitRate(mp4_fh, id);
                s->video.width = MP4GetTrackVideoWidth(mp4_fh, id);
                s->video.height = MP4GetTrackVideoHeight(mp4_fh, id);
                s->video.framerate = MP4GetTrackVideoFrameRate(mp4_fh, id);
                lms_stream_video_info_aspect_ratio_guess(&s->video);
            }

            s->next = video_info.streams;
            video_info.streams = s;
        }
        video_info.length = info.length;
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

    if (stream_type == LMS_STREAM_TYPE_AUDIO) {
        audio_info.id = finfo->id;
        audio_info.title = info.title;
        audio_info.artist = info.artist;
        audio_info.album = info.album;
        audio_info.genre = info.genre;
        audio_info.container = _get_container(mp4_fh);
        audio_info.trackno = info.trackno;

        _fill_audio_dlna_profile(&audio_info);

        r = lms_db_audio_add(plugin->audio_db, &audio_info);
    } else {
        video_info.id = finfo->id;
        video_info.title = info.title;
        video_info.artist = info.artist;
        video_info.container = _get_container(mp4_fh);

        _fill_video_dlna_profile(&video_info);

        r = lms_db_video_add(plugin->video_db, &video_info);
    }

fail:
    MP4TagsFree(tags);

    free(info.title.str);
    free(info.artist.str);
    free(info.album.str);
    free(info.genre.str);

    while (video_info.streams) {
        struct lms_stream *s = video_info.streams;
        video_info.streams = s->next;
        if (s->type == LMS_STREAM_TYPE_VIDEO) {
            free(s->codec.str); /* ugly, but h264 needs alloc */
            free(s->video.aspect_ratio.str);
        }
        free(s->lang.str);
        free(s);
    }

    MP4Close(mp4_fh, 0);

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
        "MP4 files (MP4, M4A, MOV, QT, 3GP)",
        PV,
        _authors,
        "http://lms.garage.maemo.org"
    };

    return &info;
}
