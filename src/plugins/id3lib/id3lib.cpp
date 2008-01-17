/**
 * Copyright (C) 2007 by INdT
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
 * @author Gustavo Sverzut Barbieri <gustavo.barbieri@openbossa.org>
 */

/**
 * @brief
 *
 * Reads ID3 tags from MP3 using id3lib.
 *
 * @todo: write a faster module to replace this one, using no external libs.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE 600
#include <lightmediascanner_plugin.h>
#include <lightmediascanner_utils.h>
#include <lightmediascanner_db.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <id3/tag.h>

extern "C" {

static void
_id3lib_get_string(const ID3_Frame *frame, ID3_FieldID field_id, struct lms_string_size *s)
{
  ID3_Field* field;
  ID3_TextEnc enc;
  size_t size;

  field = frame->GetField(field_id);
  if (!field)
    return;

  enc = field->GetEncoding();
  field->SetEncoding(ID3TE_ISO8859_1);
  size = field->Size();
  if (size < 1)
    goto done;

  size++;
  s->str = (char *)malloc(size * sizeof(char));
  s->len = field->Get(s->str, size);
  if (s->len > 0)
    lms_strstrip(s->str, &s->len);

  if (s->len < 1) {
    free(s->str);
    s->str = NULL;
    s->len = 0;
  }

 done:
  field->SetEncoding(enc);
}

static unsigned int
_id3lib_get_string_as_int(const ID3_Frame *frame, ID3_FieldID field_id)
{
  char buf[32];
  ID3_Field* field;
  size_t size;

  field = frame->GetField(field_id);
  if (!field)
    return 0;

  size = field->Get(buf, sizeof(buf));
  if (size > 0)
    return atoi(buf);

  return 0;
}

static int
_id3lib_get_data(const ID3_Tag &tag, struct lms_audio_info *info)
{
  ID3_Tag::ConstIterator *itr;
  const ID3_Frame *frame;
  int todo;

  todo = 7; /* info fields left to parse: title, artist, album, genre,
               trackno, rating, playcnt */

  itr = tag.CreateIterator();

  while ((frame = itr->GetNext()) != NULL && todo > 0) {
    ID3_FrameID fid;

    fid = frame->GetID();

    switch (fid) {
    case ID3FID_TITLE:
      if (!info->title.str) {
        _id3lib_get_string(frame, ID3FN_TEXT, &info->title);
        if (info->title.str)
          todo--;
      }
      break;

    case ID3FID_LEADARTIST:
      if (!info->artist.str) {
        _id3lib_get_string(frame, ID3FN_TEXT, &info->artist);
        if (info->artist.str)
          todo--;
      }
      break;

    case ID3FID_ALBUM:
      if (!info->album.str) {
        _id3lib_get_string(frame, ID3FN_TEXT, &info->album);
        if (info->album.str)
          todo--;
      }
      break;

    case ID3FID_CONTENTTYPE:
      if (!info->genre.str) {
        _id3lib_get_string(frame, ID3FN_TEXT, &info->genre);
        if (info->genre.str)
          todo--;
      }
      break;

    case ID3FID_TRACKNUM:
      if (!info->trackno) {
        info->trackno = _id3lib_get_string_as_int(frame, ID3FN_TEXT);
        if (info->trackno)
          todo--;
      }
      break;

    case ID3FID_POPULARIMETER:
      if (!info->rating) {
        info->rating = frame->GetField(ID3FN_RATING)->Get();
        if (info->rating)
          todo--;
      }
      if (!info->playcnt) {
        info->playcnt = frame->GetField(ID3FN_COUNTER)->Get();
        if (info->playcnt)
          todo--;
      }
      break;

    default:
      break;
    }
  }

  delete itr;
  return 0;
}

static const char _name[] = "id3lib";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".mp3")
};

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
};

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
    struct lms_audio_info info = {0};
    ID3_Tag tag;
    int r;

    tag.Link(finfo->path);
    r = _id3lib_get_data(tag, &info);
    if (r != 0)
      goto done;

    if (!info.title.str) {
      int ext_idx;

      ext_idx = ((int)match) - 1;
      info.title.len = finfo->path_len - finfo->base - _exts[ext_idx].len;
      info.title.str = (char *)malloc((info.title.len + 1) * sizeof(char));
      memcpy(info.title.str, finfo->path + finfo->base, info.title.len);
      info.title.str[info.title.len] = '\0';
    }

    if (info.title.str)
      lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);
    if (info.artist.str)
      lms_charset_conv(ctxt->cs_conv, &info.artist.str, &info.artist.len);
    if (info.album.str)
      lms_charset_conv(ctxt->cs_conv, &info.album.str, &info.album.len);
    if (info.genre.str)
      lms_charset_conv(ctxt->cs_conv, &info.genre.str, &info.genre.len);

    info.id = finfo->id;
    r = lms_db_audio_add(plugin->audio_db, &info);

 done:
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
_finish(struct plugin *plugin,  struct lms_context *ctxt)
{
    if (plugin->audio_db)
        return lms_db_audio_free(plugin->audio_db);

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

}
