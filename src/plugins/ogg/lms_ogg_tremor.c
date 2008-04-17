/**
 * Copyright (C) 2007-2008 by INdT
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
 * @author Eduardo Lima (Etrunko) <eduardo.lima@indt.org.br>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tremor/ogg.h>

#include "lms_ogg_private.h"

ogg_sync_state *lms_create_ogg_sync(void)
{
    return ogg_sync_create();
}

void lms_destroy_ogg_sync(ogg_sync_state *osync)
{
    ogg_sync_destroy(osync);
}

ogg_stream_state *lms_create_ogg_stream(int serial)
{
    return ogg_stream_create(serial);
}

void lms_destroy_ogg_stream(ogg_stream_state *ostream)
{
    ogg_stream_destroy(ostream);
}

lms_ogg_buffer_t lms_get_ogg_sync_buffer(ogg_sync_state *osync, long size)
{
    return ogg_sync_bufferin(osync, size);
}
