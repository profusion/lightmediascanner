/**
 * Copyright (C) 2013  Intel Corporation. All rights reserved.
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
 * @author Lucas De Marchi <lucas.demarchi@intel.com>
 */

#include <stdlib.h>
#include <string.h>

#include "util.h"

struct lms_string_size str_extract_name_from_path(
    const char *path, unsigned int pathlen, unsigned int baselen,
    const struct lms_string_size *ext, struct lms_charset_conv *cs_conv)
{
    struct lms_string_size str;

    str.len = pathlen - baselen - ext->len;
    str.str = malloc(str.len + 1);
    if (!str.str)
        return (struct lms_string_size) { };
    memcpy(str.str, path + baselen, str.len);
    str.str[str.len] = '\0';
    if (cs_conv)
        lms_charset_conv(cs_conv, &str.str, &str.len);

    return str;
}

