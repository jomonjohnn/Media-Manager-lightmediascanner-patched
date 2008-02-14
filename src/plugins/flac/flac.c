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
 * flac file parser.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <lightmediascanner_plugin.h>
#include <lightmediascanner_db.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <FLAC/metadata.h>

struct plugin {
    struct lms_plugin plugin;
    lms_db_audio_t *audio_db;
};

static const char _name[] = "flac";
static const struct lms_string_size _exts[] = {
    LMS_STATIC_STRING_SIZE(".flac")
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
    struct lms_audio_info info = {0, {0}, {0}, {0}, {0}, 0, 0, 0};
    FLAC__StreamMetadata *tags = 0;
    char *str;
    int i, len, r;

    if (!FLAC__metadata_get_tags(finfo->path, &tags)) {
        fprintf(stderr, "ERROR: cannot retrieve file %s tags\n", finfo->path);
        return -1;
    }

    for (i = 0; i < tags->data.vorbis_comment.num_comments; i++) {
        str = (char *) tags->data.vorbis_comment.comments[i].entry;
        len = tags->data.vorbis_comment.comments[i].length;
        if (strncmp(str, "TITLE=", 6) == 0) {
            info.title.str = str + 6;
            info.title.len = len;
        }
        else if (strncmp(str, "ARTIST=", 7) == 0) {
            info.artist.str = str + 7;
            info.artist.len = len;
        }
        else if (strncmp(str, "ALBUM=", 6) == 0) {
            info.album.str = str + 6;
            info.album.len = len;
        }
        else if (strncmp(str, "GENRE=", 6) == 0) {
            info.genre.str = str + 6;
            info.genre.len = len;
        }
        else if (strncmp(str, "TRACKNUMBER=", 12) == 0)
            info.trackno = atoi(str + 12);
    }

    lms_string_size_strip_and_free(&info.title);
    lms_string_size_strip_and_free(&info.artist);
    lms_string_size_strip_and_free(&info.album);
    lms_string_size_strip_and_free(&info.genre);

    if (!info.title.str) {
        int ext_idx;
        ext_idx = ((int)match) - 1;
        info.title.len = finfo->path_len - finfo->base - _exts[ext_idx].len;
        info.title.str = malloc((info.title.len + 1) * sizeof(char));
        memcpy(info.title.str, finfo->path + finfo->base, info.title.len);
        info.title.str[info.title.len] = '\0';
    }
    lms_charset_conv(ctxt->cs_conv, &info.title.str, &info.title.len);

    if (info.artist.str)
        lms_charset_conv(ctxt->cs_conv, &info.artist.str, &info.artist.len);
    if (info.album.str)
        lms_charset_conv(ctxt->cs_conv, &info.album.str, &info.album.len);
    if (info.genre.str)
        lms_charset_conv(ctxt->cs_conv, &info.genre.str, &info.genre.len);

#if 0
    fprintf(stderr, "file %s info\n", finfo->path);
    fprintf(stderr, "\ttitle='%s'\n", info.title.str);
    fprintf(stderr, "\tartist='%s'\n", info.artist.str);
    fprintf(stderr, "\talbum='%s'\n", info.album.str);
    fprintf(stderr, "\tgenre='%s'\n", info.genre.str);
    fprintf(stderr, "\ttrack number='%d'\n", info.trackno);
#endif

    info.id = finfo->id;
    r = lms_db_audio_add(plugin->audio_db, &info);

    FLAC__metadata_object_delete(tags);

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
_finish(struct plugin *plugin, struct lms_context *ctxt)
{
    if (plugin->audio_db)
        lms_db_audio_free(plugin->audio_db);
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